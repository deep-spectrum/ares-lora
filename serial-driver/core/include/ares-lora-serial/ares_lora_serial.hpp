/**
 * @file ares_lora_serial.hpp
 *
 * @brief Ares Lora serial driver for the host computer.
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
#include <random>
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
 * @struct AresSerialConfigs
 *
 * Serial driver configurations.
 */
struct AresSerialConfigs {
    AresSerialConfigs() = default;

    /**
     * Construct from Python kwargs.
     * @param kwargs Python keyword arguments.
     */
    explicit AresSerialConfigs(const py::kwargs &kwargs);

    /**
     * The serial port.
     */
    std::string port;

    /**
     * Serial reception timeout.
     */
    std::chrono::milliseconds serial_timeout = 100ms;

    /**
     * Default response timeout.
     */
    std::chrono::milliseconds response_timeout = 2000ms;

    /**
     * The period to poll the input buffer at.
     */
    std::chrono::milliseconds rx_period = 100ms;

    /**
     * Designates the driver as the master node.
     */
    bool master = false;

    /**
     * Callback function for start events.
     * Parameters: (seconds, nanoseconds, source ID, broadcasted,
     * sequence count, packet id).
     */
    std::function<void(int64_t, uint64_t, uint16_t, bool, uint8_t, uint16_t)>
        start_callback = nullptr;

    /**
     * Callback for heartbeat events.
     * Parameters: (source ID, ready, broadcasted).
     */
    std::function<void(uint16_t, bool, bool)> heartbeat_callback = nullptr;

    /**
     * Callback for claim events.
     * Parameters: (Master node ID).
     */
    std::function<void(uint16_t)> claim_callback = nullptr;

    /**
     * Log event callback.
     * Parameters: (source ID, Log ID, chunk ID, number of chunks, message).
     */
    std::function<void(uint16_t, uint16_t, uint8_t, uint8_t,
                       const std::string &)>
        log_callback = nullptr;

    /**
     * Alpha parameter for the Gamma distribution.
     */
    double alpha = 1.0;

    /**
     * Beta parameter for the Gamma distribution.
     */
    double beta = 2.0;
};

/**
 * @struct AresLoraConfig
 *
 * LoRa modem configurations.
 */
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
    [[nodiscard]] AresFrame generate_frame() const;
};

/**
 * @class AresSerial
 *
 * Serial driver class for communicating with Ares LoRa devices.
 */
class AresSerial {
  public:
    /**
     * Constructor.
     * @param configs The configurations for the driver.
     */
    explicit AresSerial(const AresSerialConfigs &configs);

    /**
     * Destructor. Automatically stops the driver and closes any serial
     * connections.
     */
    ~AresSerial();

    /**
     * Configure a setting on firmware.
     * @param id The setting ID.
     * @param value The new value of the setting.
     * @return The ACK'ed error code from the firmware.
     */
    int setting_set(uint16_t id, uint32_t value);

    /**
     * Retrieve a setting configuration from firmware.
     * @param id The setting ID.
     * @return py::tuple<setting value, ACK'ed error code>
     */
    py::tuple setting_get(uint16_t id);

    /**
     * Send a start message over the LoRa network.
     * @param sec The seconds part of the start time.
     * @param nsec The nanoseconds part of the start time.
     * @param id The ID to send the start message to. Ignored if broadcast is
     * set.
     * @param broadcast Broadcast the start time to all the listening nodes.
     * @return The ACK'ed error code from firmware.
     */
    int send_start(int64_t sec, uint64_t nsec, uint16_t id, bool broadcast);

    /**
     * Configure the LoRa modem.
     * @param config The new LoRa modem configurations.
     * @return The ACK'ed error code from the firmware.
     */
    int lora_config(const AresLoraConfig &config);

    /**
     * Set the firmware response timeout.
     * @param timeout The new response timeout.
     */
    void set_response_timeout(const std::chrono::milliseconds &timeout);

    /**
     * Retrieve the firmware response timeout.
     * @return The firmware response timeout.
     */
    [[nodiscard]] std::chrono::milliseconds get_response_timeout() const;

    /**
     * Set or retrieve the LED state from firmware.
     * @param state The new LED state for the firmware or a fetch state request.
     * @return py::tuple<Fetch LED state | Default LED state, ACK'ed error code>
     */
    py::tuple led(uint8_t state);

    /**
     * Send a heartbeat message over the LoRa network.
     * @param ready Flag indicating the system is ready to collect data.
     * @param tx_cnt The amount of times to transmit the heartbeat.
     * @return The ACK'ed error code from the firmware.
     */
    int send_heartbeat(bool ready, uint8_t tx_cnt);

    /**
     * Send a logging message over the LoRa network.
     * @param log_msg The log message to send.
     * @param broadcast Flag indicating if the log message should be
     * broadcasted.
     * @param tx_cnt The number of times to send the message or the max
     * attempts.
     * @param id The ID to direct the message to.
     * @return py::tuple<ACK'ed error code, ...>.
     *
     * @note If `broadcast` is `true`, `id` is ignored.
     * @note If `broadcast` is `false`, and id is `0`, then `id` will be
     * overridden to be the claimed master node. If there is no claimed master
     * node, then the `broadcast` flag will be overridden to be `true`.
     * @note `tx_cnt` is the number of times the message is sent when
     * broadcasting. If not broadcasting, it is the maximum number of attempts
     * to exchange a logging message chunk with the destination node.
     */
    py::tuple send_log(const std::string &log_msg, bool broadcast,
                       uint8_t tx_cnt, uint16_t id);

    /**
     * Retrieve version information from the firmware.
     * @return py::tuple<app version tuple, ncs version tuple, kernel version
     * tuple>
     */
    py::tuple version();

    /**
     * Set the logging level for the core module.
     * @param level The new logging level.
     */
    void set_logging_level(uint32_t level);

    /**
     * Start driver execution.
     */
    void start();

    /**
     * Stop driver execution.
     */
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
        AresFrame::ResponseTypes payload;
    };

    void _publish_response(const AresFrame::Decoded &frame);
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
    ares::bounded_queue<AresFrame::Decoded, 10, true> _frame_q;

    Task<void()> _processing_task;
    ares::bounded_queue<AresResponse> _response_queue;

    std::recursive_mutex _serial_lock;

    std::function<void(int64_t, uint64_t, uint16_t, bool, uint8_t, uint16_t)>
        _start_callback = nullptr;
    void _start_event(const AresFrame::Start &start_frame) const;

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
    void _heartbeat_event(const AresFrame::Heartbeat &heartbeat);
    static void _heartbeat_handler(Work *work);
    HeartbeatWork _heartbeat_work;
    int _heartbeat_claim_host(uint16_t destination_id);

    std::function<void(uint16_t)> _claim_callback = nullptr;
    void _claim_event(const AresFrame::Claim &claim);

    SpinLock _log_spinlock;
    uint16_t _log_id = 0;
    ares::bounded_queue<AresFrame::LogAck> _log_ack_signal;
    void _log_ack_event(const AresFrame::LogAck &ack);
    bool _log_ack_event_wait(const std::chrono::milliseconds &timeout,
                             size_t part, size_t num_parts, uint16_t id);
    void _send_log_frame_directed(AresFrame &frame,
                                  const std::chrono::milliseconds &ack_timeout,
                                  size_t max_attempts,
                                  std::vector<AresResponse> &responses,
                                  uint16_t target);
    std::gamma_distribution<double> _mac_backoff;
    std::random_device _rd;
    std::mt19937 _generator;
    void _handle_ack(uint16_t target, bool acked);
    std::function<void(uint16_t, uint16_t, uint8_t, uint8_t,
                       const std::string &msg)>
        _log_callback = nullptr;
    void _log_event(const AresFrame::Log &log) const;

    static py::tuple _decode_version(uint32_t version_num);

    static void _debug_event(const AresFrame::Dbg &msg);
};

#endif // ARES_ARES_LORA_SERIAL_HPP
