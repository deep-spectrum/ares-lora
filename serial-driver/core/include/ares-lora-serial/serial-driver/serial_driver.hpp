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
#include <ares/synchronization/shared_flag.hpp>
#include <ares/work-q/task.hpp>
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

/**
 * @class AresThreadTerminate
 * Exception for indicating that a thread should terminate its execution.
 */
class AresThreadTerminate : public std::exception {
  public:
    /**
     * Constructor.
     */
    AresThreadTerminate() = default;

    /**
     * Exception message.
     * @return The exception message.
     */
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

/**
 * @class AresSerial
 * Serial driver class.
 */
class AresSerial {
  public:
    /**
     * Constructor.
     * @param port The serial port the device is on.
     * @param kwargs Keyword arguments.
     */
    explicit AresSerial(const std::string &port, const py::kwargs &kwargs);

    /**
     * Destructor.
     */
    ~AresSerial();

    /**
     * @defgroup mcu Microcontroller commands.
     * @{
     */

    /**
     * Retrieve or save a setting to the microcontroller flash.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[error code, value | None]
     */
    py::tuple setting(const py::kwargs &kwargs);

    /**
     * Reconfigure the LoRa radio.
     * @param[in] config The new LoRa configurations.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[error code, None]
     */
    py::tuple lora_config(const AresLoraConfig &config,
                          const py::kwargs &kwargs);

    /**
     * Set or retrieve the LED action.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[error code, state | None]
     */
    py::tuple led(const py::kwargs &kwargs);

    /**
     * Retrieve the firmware version.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[error code, tuple[versions]]
     */
    py::tuple version(const py::kwargs &kwargs);

    /**
     * Reboot the microcontroller.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[error code, None]
     */
    py::tuple reboot(const py::kwargs &kwargs);

    /**
     * @}
     */

    /**
     * @defgroup lora LoRa commands.
     * @{
     */

    /**
     * Send the start message over LoRa.
     * @param[in] second The second to start on
     * @param[in] usec The microsecond to start on.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[tuple[code], tuple[bool] | None]
     */
    py::tuple start(int64_t second, uint64_t usec, const py::kwargs &kwargs);

    /**
     * Poll a node for ready status.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[tuple[code], tuple[bool] | None]
     */
    py::tuple poll(const py::kwargs &kwargs);

    /**
     *  Send a log message over LoRa.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[tuple[code], tuple[bool] | None]
     */
    py::tuple log(const py::kwargs &kwargs);

    /**
     * Send an abort message.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[tuple[code], tuple[bool] | None]
     */
    py::tuple abort(const py::kwargs &kwargs);

    /**
     * Send node configurations over LoRa.
     * @param[in] kwargs Python keyword arguments.
     * @return dict[config, bool]
     */
    py::dict node_config(const py::kwargs &kwargs);

    /**
     * Poll a node for its configurations over LoRa.
     * @param[in] args Python arguments.
     * @param[in] kwargs Python keyword arguments.
     * @return dict[config, value]
     */
    py::dict node_config_poll(const py::args &args, const py::kwargs &kwargs);

    /**
     * Notify a node that the run is ready.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[tuple[code], tuple[bool]]
     */
    py::tuple notify_run_ready(const py::kwargs &kwargs);

    /**
     * @}
     */

    /**
     * @defgroup ble BLE commands.
     * @{
     */

    /**
     * Set or retrieve the BLE state.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[tuple[code], tuple[state|None] | None]
     */
    py::tuple ble_state(const py::kwargs &kwargs);

    /**
     * Force the BLE to disconnect.
     * @param[in] kwargs Python keyword arguments.
     * @return tuple[tuple[code], None]
     */
    py::tuple ble_disconnect(const py::kwargs &kwargs);
    // TODO: This needs some more thought put into it.
    // py::tuple ble_send_image(const py::bytes &image);

    /**
     * @}
     */

    /**
     * Set the ready flag.
     * @param[in] new_state The new state.
     */
    void set_ready(bool new_state);

    /**
     * Retrieve the ready state.
     * @return The current ready state.
     */
    [[nodiscard]] bool get_ready();

    /**
     * Register logging redirects.
     *
     * @param[in] dbg Debug message callback.
     * @param[in] info Info message callback.
     * @param[in] warn Warning message callback.
     * @param[in] error Error message callback.
     * @param[in] crit Critical message callback.
     * @param[in] get_level Get level callback.
     * @param[in] set_level Set level callback.
     */
    void register_logger_callbacks(
        const std::function<void(const std::string &)> &dbg,
        const std::function<void(const std::string &)> &info,
        const std::function<void(const std::string &)> &warn,
        const std::function<void(const std::string &)> &error,
        const std::function<void(const std::string &)> &crit,
        const std::function<long()> &get_level,
        const std::function<void(long)> &set_level);

    /**
     * Set the logging level for the core module.
     * @param[in] level The new logging level.
     */
    void set_logging_level(uint32_t level);

    /**
     * Retrieve the current logger level.
     * @return The current log level.
     */
    long get_log_level();

    /**
     * Wait for a start message to be received.
     * @return tuple[seconds, useconds, src_id, broadcast, seq_cnt, packet_id]
     */
    py::tuple wait_start_event();

    /**
     * Wait for a log message to be received.
     * @return tuple[src_id, log_id, chunk, num_chunks, msg]
     */
    py::tuple wait_log_event();

    /**
     * Wait for any LoRa packet reception.
     * @return tuple[seq_cnt, packet_id, source_id]
     */
    py::tuple wait_packet_rx_event();

    /**
     * Wait for LoRa transmission to finish.
     * @return tx_count
     */
    uint32_t wait_packet_tx_event();

    /**
     * Wait for a BLE connection event to be received.
     * @return @p true if a BLE connection was established, @p false otherwise.
     */
    bool wait_ble_connection_event();

    /**
     * Wait for BLE service subscription status changes.
     * @return tuple[subscription statuses, ...]
     */
    py::tuple wait_ble_subscribe_event();

    /**
     * Wait in the waiting room of unplanned parenthood.
     * @return tuple[broadcast, src_id]
     */
    py::tuple wait_abortion_event();

    /**
     * Wait for the event indicating that the run is ready.
     * @return tuple[src_id, broadcast]
     */
    py::tuple wait_run_ready_event();

    /**
     * Start the driver.
     */
    void start_driver();

    /**
     * Stop the driver.
     */
    void stop_driver();

    /**
     * Retrieve the node configurations.
     * @return dict[config, value]
     */
    py::dict get_node_config();

    /**
     * Cancel the event queues.
     */
    void cancel_events();

    friend class FrameDispatcher;

  private:
    // High level stuff
    Serial::Serial _serial;
    std::mutex _command_lock;
    std::exception_ptr _exception;

    std::mutex _ready_mtx;
    bool _ready = false;
    uint16_t _log_id = 0;

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

    // Overall Task related stuff
    std::recursive_mutex _serial_lock;
    ares::SharedFlag _tasks_running;

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

    // Lora response task stuff
    ares::Task<void()> _lora_response_task;
    ares::bounded_queue<AresFrame::Frame::TxTypes, 10> _lora_response_q;
    std::chrono::milliseconds _send_lora_response_timeout = 10s;
    static void _lora_responses_check_fw_responses(
        const std::vector<FrameResponse> &responses,
        const AresFrame::AresFrameType &sent_type);
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
    ares::bounded_queue<std::unique_ptr<AresFrame::Frame::AckTypes>, 10>
        _ack_queue;
    bool _stop_event_queues();

    struct EventDispatcher {
        AresSerial &self;

        template <typename T, size_t size, bool overwrite>
        static void put_no_except(
            const T &event,
            ares::bounded_queue<std::unique_ptr<T>, size, overwrite> &q,
            const std::chrono::milliseconds &timeout) {
            try {
                q.put(std::make_unique<T>(event), timeout);
            } catch (ares::queue_exception &) {
                // nop
            }
        }

        void operator()(const AresFrame::Start &event) const;
        void operator()(const AresFrame::Heartbeat &event) const;
        void operator()(const AresFrame::Poll &event) const;
        void operator()(const AresFrame::Log &event) const;
        void operator()(const AresFrame::LogAck &event) const;
        void operator()(const AresFrame::Dbg &event) const;
        void operator()(const AresFrame::PktRx &event) const;
        void operator()(const AresFrame::PktTx &event) const;
        void operator()(const AresFrame::BleConnect &event) const;
        void operator()(const AresFrame::BleSubscribed &event) const;
        void operator()(const AresFrame::Abort &event) const;
        void operator()(const AresFrame::NodeConfig &event) const;
        void operator()(const AresFrame::NodeConfigPoll &event) const;
        void operator()(const AresFrame::NodeConfigResponse &event) const;
        void operator()(const AresFrame::NodeReady &event) const;
        void operator()(const AresFrame::LoraAck &event) const;

        template <typename T> void operator()(const T &event) const {
            FrameResponse response;
            if constexpr (std::is_same_v<T, AresFrame::Ack>) {
                response.type = FrameResponse::ACK;
            } else if constexpr (std::is_same_v<T, AresFrame::FramingError>) {
                response.type = FrameResponse::BAD_FRAME;
            } else {
                response.type = FrameResponse::COMMAND_SPECIFIC;
            }

            if constexpr (std::is_constructible_v<decltype(response.payload),
                                                  T>) {
                response.payload = event;
                self._response_queue.put(response);
            } else {
                assert(false);
            }
        }
    };

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

    // BLE Stuff
    struct BleInfo {
        struct {
            bool chunk = false;
            bool image = false;
        } subscriptions;
        bool connected = false;
        size_t mtu_size = 0;
    };
    BleInfo _ble_info;
};

/**
 * Acquire the Python GIL and check if any Python exceptions were thrown.
 * @throws py::error_already_set
 */
void check_python_errors();

#endif // ARES_SERIAL_DRIVER_HPP
