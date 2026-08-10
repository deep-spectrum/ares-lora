/**
 * @file serial_driver.hpp
 *
 * @brief
 *
 * @date 8/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SERIAL_DRIVER_HPP
#define ARES_SERIAL_DRIVER_HPP

#include <ares-lora-serial/frames/frame.hpp>
#include <ares/data-structures/queue.hpp>
#include <ares/datetime/datetime.hpp>
#include <ares/serial/serial.hpp>
#include <ares/synchronization/semaphore.hpp>
#include <ares/work-q/task.hpp>
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

/**
 * @class AresTimeoutError
 * Timeout exception class for the Ares serial driver core library.
 */
class AresTimeoutError : public std::exception {
  public:
    /**
     * Constructor.
     * @param msg The error message.
     */
    explicit AresTimeoutError(std::string msg) : _msg(std::move(msg)) {}

    /**
     * Retrieve the error message.
     * @return The error message.
     */
    [[nodiscard]] const char *what() const noexcept override {
        return _msg.c_str();
    }

  private:
    std::string _msg;
};

class AresThreadTerminate : public std::exception {
  public:
    AresThreadTerminate() = default;

    [[nodiscard]] const char *what() const noexcept override {
        return "Thread terminate signal";
    }
};

struct AresLoraConfig {
    AresLoraConfig() = default;

    /**
     * Construct from Python kwargs.
     * @param kwargs Python keyword arguments.
     */
    explicit AresLoraConfig(const py::kwargs &kwargs);

    /**
     * Frequency in Hz to use for transceiving
     */
    uint32_t frequency = 0;

    /**
     * Length of the preamble.
     */
    uint16_t preamble_length = 0;

    /**
     * The bandwidth to use for transceiving.
     */
    uint8_t bandwidth = 0;

    /**
     * The data-rate to use for transceiving.
     */
    uint8_t datarate = 0;

    /**
     * The coding rate to use for transceiving.
     */
    uint8_t coding_rate = 0;

    /**
     * TX-power in dBm to use for transmission.
     */
    int8_t tx_power = 0;

    /**
     * Generate an AresFrame from the object.
     * @return The frame object generated.
     */
    [[nodiscard]] AresFrame::Frame generate_frame() const;
};

class AresSerial {
  public:
    explicit AresSerial(const std::string &port, const py::kwargs &kwargs);

    ~AresSerial();

    // mcu
    py::tuple setting(const py::kwargs &kwargs);
    int lora_config(const AresLoraConfig &config, const py::kwargs &kwargs);
    py::tuple led(const py::kwargs &kwargs);
    py::tuple version();
    int reboot();

    // lora
    int start(const py::kwargs &kwargs);
    py::tuple poll(const py::kwargs &kwargs);
    py::tuple log(const py::kwargs &kwargs);
    int abort(const py::kwargs &kwargs);
    py::dict node_config(const py::kwargs &kwargs);
    py::dict node_config_poll(const py::kwargs &kwargs);
    int notify_run_ready(const py::kwargs &kwargs);

    // ble
    py::tuple ble_state(uint8_t value);
    int ble_disconnect();
    py::tuple ble_send_image(const py::bytes &image);

    // getters/setters
    void set_ready(bool new_state);
    [[nodiscard]] bool get_ready() const;

    // Driver logging
    void register_logger_callbacks(
        const std::function<void(const std::string &)> &dbg,
        const std::function<void(const std::string &)> &info,
        const std::function<void(const std::string &)> &warn,
        const std::function<void(const std::string &)> &error,
        const std::function<void(const std::string &)> &crit,
        const std::function<long()> &get_level,
        const std::function<void(long)> &set_level);
    void set_logging_level(uint32_t level);
    long get_log_level();

    // Event waiting
    py::tuple wait_start_event();
    py::tuple wait_log_event();
    py::tuple wait_packet_rx_event();
    uint32_t wait_packet_tx_event();
    bool wait_ble_connection_event();
    py::tuple wait_ble_subscribe_event();
    py::tuple wait_abortion_event();
    py::tuple wait_run_ready_event();

    // driver utilities
    void start_driver();
    void stop_driver();
    py::dict get_node_config();
    void cancel_events();

  private:
    // High level stuff
    Serial::Serial _serial;
    std::mutex _command_lock;
    std::exception_ptr _exception;
    bool _ready = false;

    void _check_crash();

    // Frame stuff
    struct FrameResponse {
        enum ResponseType {
            COMMAND_SPECIFIC,
            ACK,
            BAD_FRAME,
        };

        ResponseType type;
        AresFrame::Frame::ResponseTypes payload;
    };

    // Task related stuff
    std::recursive_mutex _serial_lock;
    std::atomic_bool _tasks_running = false;

    // Receive task stuff
    ares::Task<void()> _rx_task;
    ares::bounded_queue<AresFrame::Frame, 10, true> _frame_q;
    std::chrono::milliseconds _rx_period = 100ms;
    void _process_rx_buffer(std::vector<uint8_t> &buf);
    void _read_serial_helper();
    void _read_serial();

    // Processing task stuff
    ares::Task<void()> _processing_task;
    ares::bounded_queue<FrameResponse> _response_queue;
    void _process_frames_helper();
    void _process_frames();

    // Send frame stuff
    void _send_frame(AresFrame::Frame &frame,
                     const std::chrono::milliseconds &timeout,
                     std::vector<FrameResponse> &responses);
    void _send_frame_released(AresFrame::Frame &frame,
                              const std::chrono::milliseconds &timeout,
                              std::vector<FrameResponse> &responses);
    void _send_frame_released(const std::vector<uint8_t> &buf);
    FrameResponse _wait_response(const std::chrono::milliseconds &timeout);
    FrameResponse
    _wait_response_timeout(const std::chrono::milliseconds &timeout);
    FrameResponse _wait_response_forever();

    // Lora response task stuff
    ares::Task<void()> _lora_response_task;
    ares::bounded_queue<AresFrame::Frame, 10> _lora_response_q;
    std::chrono::milliseconds _send_lora_response_timeout = 10s;
    static void _lora_responses_check_fw_responses(
        const std::vector<FrameResponse> &responses,
        const AresFrame::Frame &sent_frame);
    void _send_lora_responses_helper();
    void _send_lora_responses();

    // event queues
    ares::bounded_queue<std::unique_ptr<AresFrame::Start>, 5> _start_event_q;
    ares::bounded_queue<std::unique_ptr<AresFrame::Log>, 100> _log_event_q;
    ares::bounded_queue<std::unique_ptr<AresFrame::PktRx>, 500> _pkt_rx_event_q;
    ares::bounded_queue<std::unique_ptr<AresFrame::PktTx>, 3> _pkt_tx_event_q;
    ares::bounded_queue<std::unique_ptr<AresFrame::BleConnect>, 2>
        _ble_connect_event_q;
    ares::bounded_queue<std::unique_ptr<AresFrame::BleSubscribed>, 10>
        _ble_subscribed_event_q;
    ares::bounded_queue<std::unique_ptr<AresFrame::Abort>, 3> _abortion_event_q;
    ares::bounded_queue<std::unique_ptr<AresFrame::NodeReady>, 3>
        _run_ready_event_q;
    bool _stop_event_queues();

    // Node configuration stuff
    struct NodeConfigs {
        NodeConfigs() = default;

        ares::DateTime save_folder;
        double bandwidth = 0;
        double center_freq = 0;
        double ref_level = 0;
        uint32_t duration = 0;

        ares::semaphore<> sem{};
    };
    NodeConfigs _node_configs;
};

#endif // ARES_SERIAL_DRIVER_HPP
