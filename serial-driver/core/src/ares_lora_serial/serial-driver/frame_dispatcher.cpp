/**
 * @file frame_dispatcher.cpp
 *
 * @brief
 *
 * @date 8/12/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

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

static void set_lora_broadcast(AresFrame::Frame::TxTypes &payload,
                               bool broadcast) {
    std::visit(
        [&broadcast](auto &obj) {
            if constexpr (has_member_broadcast_v<std::decay_t<decltype(obj)>>) {
                obj.broadcast = broadcast;
            }
        },
        payload);
}

py::tuple FrameDispatcher::send_frame(AresFrame::Frame::TxTypes &payload) {
    set_lora_destination_id(payload, destination);
    set_lora_broadcast(payload, broadcast);

    AresFrame::Frame frame(payload);
    std::vector<AresSerial::FrameResponse> responses;

    _send_frame(frame, response_timeout, responses);

    // todo
    return py::tuple();
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

    do {
        lock.lock();
        frame.serialize(buf);
        _serial._response_queue.clear();
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
