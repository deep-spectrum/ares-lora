/**
 * @file ares_lora_serial.hpp
 *
 * @brief
 *
 * @date 3/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_ARES_LORA_SERIAL_HPP
#define ARES_ARES_LORA_SERIAL_HPP

#include <ares-lora-serial/ares_frame.hpp>
#include <ares/queue.hpp>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <pybind11/pybind11.h>
#include <serial/serial.hpp>
#include <string>

namespace py = pybind11;
using namespace std::chrono_literals;

class AresTimeoutError : public std::exception {
  public:
    explicit AresTimeoutError(const std::string &msg) : _msg(msg) {}

    const char *what() const noexcept override { return _msg.c_str(); }

  private:
    std::string _msg;
};

struct AresSerialConfigs {
    AresSerialConfigs() = default;
    explicit AresSerialConfigs(const py::kwargs &kwargs);

    std::string port;
    std::chrono::milliseconds serial_timeout = 100ms;
    std::chrono::milliseconds response_timeout = 2000ms;
    std::chrono::milliseconds rx_period = 100ms;
    std::function<void(int64_t, uint64_t, uint16_t, bool, uint8_t, uint16_t)>
        start_callback = nullptr;
};

class AresSerial {
  public:
    explicit AresSerial(const AresSerialConfigs &configs);
    ~AresSerial();

    // returns error code
    int setting_set(uint16_t id, uint32_t value);
    // returns (value, error code)
    py::tuple setting_get(uint16_t id);

    void start();
    void stop();

  private:
    Serial::Serial _serial;

    std::chrono::milliseconds _response_timeout;
    std::chrono::milliseconds _rx_period;

    void _process_frames_helper();
    void _process_frames();

    void _process_rx_buffer(std::vector<uint8_t> &buf);
    void _read_serial_helper();
    void _read_serial();

    struct AresResponse {
        enum ResponseType {
            COMMAND_SPECIFIC, // Command specific response (example: setting
                              // command)
            ACK,              // AresFrameAckErrorCode
            BAD_FRAME,        // AresFrameFramingError
        };

        ResponseType type;
        AresFrame::AresFrameResponseTypes payload;
    };

    void _publish_response(const AresFrame::AresFrameDecoded &frame);
    AresResponse _send_frame(AresFrame &frame);

    struct Task {
        std::packaged_task<void()> task;
        std::future<void> future;
        std::thread thread;
    };

    std::atomic_bool _tasks_running = false;

    Task _rx_task;
    ares::bounded_queue<AresFrame::AresFrameDecoded, 10, true> _frame_q;

    Task _processing_task;
    ares::bounded_queue<AresResponse> _response_queue;

    std::recursive_mutex _serial_lock;

    std::function<void(int64_t, uint64_t, uint16_t, bool, uint8_t, uint16_t)>
        _start_callback = nullptr;
    void _start_event(const AresFrame::AresFrameStart &start_frame) const;

    static void _handle_bad_frame(const AresResponse &response);
};

#endif // ARES_ARES_LORA_SERIAL_HPP
