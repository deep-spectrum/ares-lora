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
#include <ares/data-structures/queue.hpp>
#include <ares/serial/serial.hpp>
#include <ares/synchronization/semaphore.hpp>
#include <ares/synchronization/spinlock.hpp>
#include <ares/work-q/task.hpp>
#include <ares/work-q/work_q.hpp>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <pybind11/pybind11.h>
#include <string>
#include <utility>

namespace py = pybind11;
using namespace std::chrono_literals;

class AresTimeoutError : public std::exception {
  public:
    explicit AresTimeoutError(std::string msg) : _msg(std::move(msg)) {}

    [[nodiscard]] const char *what() const noexcept override {
        return _msg.c_str();
    }

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
    bool master = false;
    std::function<void(int64_t, uint64_t, uint16_t, bool, uint8_t, uint16_t)>
        start_callback = nullptr;
    std::function<void(uint16_t, bool, bool)> heartbeat_callback = nullptr;
    std::function<void(uint16_t)> claim_callback = nullptr;
    std::function<void(uint16_t, uint16_t, uint8_t, uint8_t, const std::string &)>
        log_callback = nullptr;
};

struct AresLoraConfig {
    AresLoraConfig() = default;
    explicit AresLoraConfig(const py::kwargs &kwargs);

    uint32_t frequency = 0;
    uint16_t preamble_length = 0;
    uint8_t bandwidth = 0;
    uint8_t datarate = 0;
    uint8_t coding_rate = 0;
    int8_t tx_power = 0;

    [[nodiscard]] AresFrame generate_frame() const;
};

class AresSerial {
  public:
    explicit AresSerial(const AresSerialConfigs &configs);
    ~AresSerial();

    // returns error code
    int setting_set(uint16_t id, uint32_t value);
    // returns (value, error code)
    py::tuple setting_get(uint16_t id);

    // returns error code
    int send_start(int64_t sec, uint64_t nsec, uint16_t id, bool broadcast);

    // returns error code
    int lora_config(const AresLoraConfig &config);

    void set_response_timeout(const std::chrono::milliseconds &timeout);
    [[nodiscard]] std::chrono::milliseconds get_response_timeout() const;

    py::tuple led(uint8_t state);

    // returns error code
    int send_heartbeat(bool ready, uint8_t tx_cnt);

    // returns error code of all frames
    py::tuple send_log(const std::string &log_msg, bool broadcast,
                       uint8_t tx_cnt, uint16_t id);

    py::tuple version();

    void start();
    void stop();

  private:
    Serial::Serial _serial;
    WorkQ _work_q;
    SpinLock _command_lock;
    std::exception_ptr _exception;

    void _check_crash();

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
    void _send_frame(const std::vector<uint8_t> &tx);
    AresResponse _send_frame(AresFrame &frame,
                             const std::chrono::milliseconds &timeout);
    void _send_multi_frame(AresFrame &frame,
                           const std::chrono::milliseconds &timeout,
                           std::vector<AresResponse> &responses);
    AresResponse _wait_response(const std::chrono::milliseconds &timeout);
    AresResponse
    _wait_response_timeout(const std::chrono::milliseconds &timeout);
    AresResponse _wait_response_forever();

    std::atomic_bool _tasks_running = false;

    Task<void()> _rx_task;
    ares::bounded_queue<AresFrame::AresFrameDecoded, 10, true> _frame_q;

    Task<void()> _processing_task;
    ares::bounded_queue<AresResponse> _response_queue;

    std::recursive_mutex _serial_lock;

    std::function<void(int64_t, uint64_t, uint16_t, bool, uint8_t, uint16_t)>
        _start_callback = nullptr;
    void _start_event(const AresFrame::AresFrameStart &start_frame) const;

    static void _handle_bad_frame(const AresResponse &response);

    struct HeartbeatWork {
        HeartbeatWork(work_handler_t handler, AresSerial *ctx)
            : work(std::move(handler)), obj(ctx) {}
        ~HeartbeatWork() { work.work_flush(); }
        Work work;
        AresSerial *obj;
        uint16_t id = 0;
        ares::semaphore<> sem{};
    };

    bool _master;
    uint16_t _claimed_host = 0;
    std::function<void(uint16_t, bool, bool)> _heartbeat_callback = nullptr;
    void _heartbeat_event(const AresFrame::AresFrameHeartbeat &heartbeat);
    static void _heartbeat_handler(Work *work);
    HeartbeatWork _heartbeat_work;
    int _heartbeat_claim_host(uint16_t destination_id);

    std::function<void(uint16_t)> _claim_callback = nullptr;
    void _claim_event(const AresFrame::AresFrameClaim &claim);

    SpinLock _log_spinlock;
    uint16_t _log_id = 0;
    ares::bounded_queue<AresFrame::AresFrameLogAck> _log_ack_signal;
    void _log_ack_event(const AresFrame::AresFrameLogAck &ack);
    bool _log_ack_event_wait(const std::chrono::milliseconds &timeout,
                             size_t part, size_t num_parts, uint16_t id);
    void _send_log_frame_directed(AresFrame &frame,
                                  const std::chrono::milliseconds &ack_timeout,
                                  size_t max_attempts,
                                  std::vector<AresResponse> &responses,
                                  uint16_t target);
    std::function<void(uint16_t, uint16_t, uint8_t, uint8_t, const std::string &msg)>
        _log_callback = nullptr;
    void _log_event(const AresFrame::AresFrameLog &log) const;

    static py::tuple _decode_version(uint32_t version_num);

    static void _debug_event(const AresFrame::AresFrameDbg &msg);
};

#endif // ARES_ARES_LORA_SERIAL_HPP
