/**
 * @file frame_dispatcher.cpp
 *
 * @brief
 *
 * @date 8/12/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/lora/lora_base.hpp>
#include <ares-lora-serial/serial-driver/frame_dispatcher.hpp>
#include <string>
#include <type_traits>

// TODO: Get the log in here

template <typename T, typename = void>
struct has_member_id : std::false_type {};

template <typename T>
struct has_member_id<T, std::void_t<decltype(std::declval<T>().id)>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_member_id_v = has_member_id<T>::value;

template <typename T, typename = void>
struct has_member_broadcast : std::false_type {};

template <typename T>
struct has_member_broadcast<T,
                            std::void_t<decltype(std::declval<T>().broadcast)>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_member_broadcast_v = has_member_broadcast<T>::value;

template <typename T, typename = void>
struct has_response_type : std::false_type {};

template <typename T>
struct has_response_type<T, std::void_t<typename T::response_type>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_response_type_v = has_response_type<T>::value;

static void set_lora_destination_id(AresFrame::Frame::TxTypes &payload,
                                    uint16_t destination) {
    std::visit(
        [&destination](auto &obj) {
            if constexpr (has_member_id_v<std::decay_t<decltype(obj)>>) {
                obj.id = destination;
            }
        },
        payload);
}

static bool set_lora_broadcast(AresFrame::Frame::TxTypes &payload,
                               bool broadcast) {
    return std::visit(
        [&broadcast](auto &obj) {
            if constexpr (has_member_broadcast_v<std::decay_t<decltype(obj)>>) {
                obj.broadcast = broadcast;
                return true;
            }
            return false;
        },
        payload);
}

static bool is_lora_frame(AresFrame::Frame::TxTypes &payload) {
    return std::visit(
        []([[maybe_unused]] const auto &obj) {
            if constexpr (std::is_base_of_v<AresFrame::Internal::LoraBase,
                                            std::decay_t<decltype(obj)>>) {
                return true;
            }
            return false;
        },
        payload);
}

static bool frame_has_response_type(AresFrame::Frame::TxTypes &payload) {
    return std::visit(
        []([[maybe_unused]] const auto &obj) {
            if constexpr (has_response_type_v<decltype(obj)>) {
                return true;
            }
            return false;
        },
        payload);
}

py::dict FrameDispatcher::send_frame(AresFrame::Frame::TxTypes &payload) {
    set_lora_destination_id(payload, destination);
    broadcast_supported = set_lora_broadcast(payload, broadcast);
    is_lora_payload = is_lora_frame(payload);
    lora_response_supported = frame_has_response_type(payload);

    std::vector<AresSerial::FrameResponse> responses;

    send_frame(payload, responses);
    // todo: how the fuck do I handle lora responses?

    py::dict ret;

    // todo: How do I process responses?
    return ret;
}

void FrameDispatcher::send_frame(
    AresFrame::Frame::TxTypes &payload,
    std::vector<AresSerial::FrameResponse> &responses) {
    AresFrame::Frame frame(payload);
    _type_dispatched = frame.type();
    _send_frame(frame, response_timeout, responses);
}

AresFrame::AresFrameType FrameDispatcher::type_dispatched() const {
    return _type_dispatched;
}

void FrameDispatcher::_send_frame(
    AresFrame::Frame &frame, const std::chrono::milliseconds &timeout,
    std::vector<AresSerial::FrameResponse> &responses) const {
    py::gil_scoped_release release;
    _send_frame_released(frame, timeout, responses);
}

void FrameDispatcher::_send_frame_released(
    AresFrame::Frame &frame, const std::chrono::milliseconds &timeout,
    std::vector<AresSerial::FrameResponse> &responses) const {
    std::unique_lock lock(_serial._command_lock, std::defer_lock);
    std::vector<uint8_t> buf;
    size_t acked_frames = 0;

    do {
        frame.serialize(buf);

        bool acked = false;

        for (size_t attempt = 0u; attempt < (retries + 1) && !acked;
             attempt++) {
            lock.lock();
            _serial._response_queue.clear();
            AresSerial::FrameResponse resp = _wait_response(response_timeout);
            lock.unlock();
            // Todo: check response (not frame error, ACK code 0)
            check_python_errors();
            // todo: check for lora ack here
        }
        // todo: insert response here

        _send_frame_released(buf);
        responses.emplace_back(_wait_response(timeout));
        lock.unlock();
        check_python_errors();
    } while (frame.frame_available());
}

void FrameDispatcher::_send_frame_released(
    const std::vector<uint8_t> &buf) const {
    // todo log
    std::unique_lock lock(_serial._serial_lock);
    _serial._serial.write(buf);
}

AresSerial::FrameResponse FrameDispatcher::_wait_response(
    const std::chrono::milliseconds &timeout) const {
    if (timeout == ares::forever) {
        return _wait_response_forever();
    }

    return _wait_response_timeout(timeout);
}

AresSerial::FrameResponse FrameDispatcher::_wait_response_timeout(
    const std::chrono::milliseconds &timeout) const {
    AresSerial::FrameResponse response;

    try {
        response = _serial._response_queue.get(timeout);
    } catch (const ares::queue_exception &exc) {
        if (exc.reason() == ares::queue_exception::QUEUE_TIMEOUT) {
            throw AresTimeoutError(exc.what());
        }
        throw;
    }

    return response;
}

AresSerial::FrameResponse FrameDispatcher::_wait_response_forever() const {
    // TODO: Add a check condition
    return _serial._response_queue.get();
}

void FrameDispatcher::_handle_bad_frame(
    const AresSerial::FrameResponse &response) {
    std::stringstream ss;
    ss << "Internal error: Bad frame received (code: "
       << std::get<AresFrame::FramingError>(response.payload).type << ")";
    throw py::buffer_error(ss.str());
}
