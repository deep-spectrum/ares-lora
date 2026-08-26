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
#include <ares/logging/log.hpp>
#include <chrono>
#include <string>
#include <type_traits>

LOG_MODULE_REGISTER(serial_logger);

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
struct has_response_type<T,
                         std::void_t<typename std::decay_t<T>::response_type>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_response_type_v = has_response_type<T>::value;

template <typename T, typename = void>
struct has_expected_response : std::false_type {};

template <typename T>
struct has_expected_response<
    T, std::void_t<decltype(std::declval<T>().expected_response())>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_expected_response_v = has_expected_response<T>::value;

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
            return has_response_type_v<decltype(obj)>;
        },
        payload);
}

CommandResponse
FrameDispatcher::send_frame(AresFrame::Frame::TxTypes &payload) {
    set_lora_destination_id(payload, destination);
    broadcast_supported = set_lora_broadcast(payload, broadcast);
    is_lora_payload = is_lora_frame(payload);
    lora_response_supported = frame_has_response_type(payload);
    _response.clear();
    _lora_responses.clear();

    AresFrame::Frame frame(payload);
    _type_dispatched = frame.type();

    std::vector<AresSerial::FrameResponse> responses;

    _send_frame(frame, response_timeout, responses);
    _process_responses(responses);

    return _response;
}

void FrameDispatcher::send_frame_released(
    AresFrame::Frame::TxTypes &payload,
    std::vector<AresSerial::FrameResponse> &responses) {
    AresFrame::Frame frame(payload);
    _type_dispatched = frame.type();
    _response.clear();
    _send_frame_released(frame, response_timeout, responses);
    _process_responses(responses);
}

AresFrame::AresFrameType FrameDispatcher::type_dispatched() const {
    return _type_dispatched;
}

bool FrameDispatcher::broadcast_msg() const { return broadcast; }

void FrameDispatcher::_send_frame(
    AresFrame::Frame &frame, const std::chrono::milliseconds &timeout,
    std::vector<AresSerial::FrameResponse> &responses) {
    py::gil_scoped_release release;
    _send_frame_released(frame, timeout, responses);
}

void FrameDispatcher::_send_frame_released(
    AresFrame::Frame &frame, const std::chrono::milliseconds &timeout,
    std::vector<AresSerial::FrameResponse> &responses) {
    if (is_lora_payload && lora_response_supported && !broadcast) {
        _send_lora_expecting_response_released(frame, timeout, responses);
    } else {
        _send_frame_normal_released(frame, timeout, responses);
    }
}

void FrameDispatcher::_send_frame_released(
    const std::vector<uint8_t> &buf) const {
    LOG_DBG_HEXDUMP(buf, buf.size(), "Sent data");
    std::unique_lock lock(_serial._serial_lock);
    _serial._serial.write(buf);
}

void FrameDispatcher::_send_frame_normal_released(
    AresFrame::Frame &frame, const std::chrono::milliseconds &timeout,
    std::vector<AresSerial::FrameResponse> &responses) const {
    std::unique_lock lock(_serial._command_lock, std::defer_lock);
    std::vector<uint8_t> buf;

    do {
        lock.lock();
        _serial._response_queue.clear();
        frame.serialize(buf);
        _send_frame_released(buf);
        responses.emplace_back(_wait_response(timeout));
        lock.unlock();
        check_python_errors();
    } while (frame.frame_available());

    _verify_responses(responses);
}

void FrameDispatcher::_send_lora_expecting_response_released(
    AresFrame::Frame &frame, const std::chrono::milliseconds &timeout,
    std::vector<AresSerial::FrameResponse> &responses) {
    std::unique_lock lock(_serial._command_lock, std::defer_lock);
    std::vector<uint8_t> buf;

    do {
        frame.serialize(buf);
        AresSerial::FrameResponse fw_response;
        bool timed_out = true;

        for (size_t attempt = 0u; attempt <= retries && timed_out; attempt++) {
            LOG_DBG("Sending LoRa frame (attempt %u out of %u)", attempt, retries);
            lock.lock();
            _serial._response_queue.clear();
            _send_frame_released(buf);
            fw_response = _wait_response(timeout);
            check_python_errors();
            (void)_verify_response(fw_response);
            timed_out = _wait_lora_response(frame);
            lock.unlock();
            LOG_DBG("Message response timed out: %d", timed_out);
        }

        responses.emplace_back(fw_response);
    } while (frame.frame_available());
}

AresFrame::Frame::AckTypes
get_expected_response(const AresFrame::Frame::TxTypes &payload) {
    return std::visit(
        [](const auto &obj) -> AresFrame::Frame::AckTypes {
            if constexpr (has_expected_response_v<decltype(obj)>) {
                return obj.expected_response();
            } else {
                throw std::runtime_error("");
            }
        },
        payload);
}

bool compare_ack(const AresFrame::Frame::AckTypes &expected,
                 const AresFrame::Frame::AckTypes &received) {
    if (expected.index() != received.index()) {
        return false;
    }

    return std::visit(
        [&received](const auto &expect) {
            using Type = std::decay_t<decltype(expect)>;
            return expect == std::get<Type>(received);
        },
        expected);
}

bool FrameDispatcher::_wait_lora_response(AresFrame::Frame &frame) {
    AresFrame::Frame::AckTypes expected =
        get_expected_response(frame.tx_payload());
    AresFrame::Frame::AckTypes received = std::monostate();
    bool timed_out = false;
    auto now = std::chrono::steady_clock::now;

    auto to_time = now() + ack_timeout;
    while (!compare_ack(expected, received) && !timed_out) {
        try {
            received = *_serial._ack_queue.get(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    ack_timeout));
        } catch (const ares::queue_exception &e) {
            timed_out = e.reason() == ares::queue_exception::QUEUE_TIMEOUT;
            continue;
        }

        timed_out = now() > to_time;
    }

    if (!timed_out) {
        _lora_responses.emplace_back(received);
    } else {
        _lora_responses.emplace_back(std::monostate());
    }

    return timed_out;
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
    return _serial._response_queue.get(check_python_errors);
}

void FrameDispatcher::_verify_responses(
    const std::vector<AresSerial::FrameResponse> &responses) const {
    for (const auto &resp : responses) {
        (void)_verify_response(resp);
    }
}

bool FrameDispatcher::_verify_response(
    const AresSerial::FrameResponse &response) const {
    bool ack_no_err = true;
    switch (response.type) {
    case AresSerial::FrameResponse::ACK: {
        ack_no_err = std::get<AresFrame::Ack>(response.payload).code == 0;
        break;
    }
    case AresSerial::FrameResponse::BAD_FRAME: {
        _handle_bad_frame(response);
        break;
    }
    case AresSerial::FrameResponse::COMMAND_SPECIFIC: {
        if (!command_specific_supported) {
            throw std::runtime_error("Received a command specific response "
                                     "when one was not expected");
        }
        break;
    }
    default: {
        throw std::runtime_error("Received an invalid response from firmware");
        break;
    }
    }

    return ack_no_err;
}

void FrameDispatcher::_handle_bad_frame(
    const AresSerial::FrameResponse &response) {
    std::stringstream ss;
    ss << "Internal error: Bad frame received (code: "
       << std::get<AresFrame::FramingError>(response.payload).type << ")";
    throw py::buffer_error(ss.str());
}

void FrameDispatcher::_process_responses(
    const std::vector<AresSerial::FrameResponse> &responses) {
    for (const auto &response : responses) {
        int err_code = 0;
        switch (response.type) {
        case AresSerial::FrameResponse::ACK: {
            err_code = std::get<AresFrame::Ack>(response.payload).code;
            break;
        }
        case AresSerial::FrameResponse::COMMAND_SPECIFIC: {
            std::visit(
                [this](auto &obj) {
                    _response.response_values.emplace_back(obj);
                },
                response.payload);
            break;
        }
        default: {
            throw std::runtime_error(
                "Invalid response found while processing responses");
            break;
        }
        }
        _response.error_codes.emplace_back(err_code);
    }

    for (const auto &response : _lora_responses) {
        std::visit(
            [this](auto &obj) { _response.response_values.emplace_back(obj); },
            response);
    }
}
