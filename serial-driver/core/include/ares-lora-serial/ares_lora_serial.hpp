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

#include <ares/queue.hpp>
#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <serial/serial.hpp>
#include <string>

struct AresSerialConfigs {
    AresSerialConfigs(const char *port, uint32_t ack_timeout,
                      uint32_t rx_period);

    std::string serial_port;
    uint32_t timeout;
    uint32_t rx_period;
};

class AresSerial {
  public:
    explicit AresSerial(const AresSerialConfigs &configs);
    ~AresSerial();

    void setting_set(uint16_t id, uint32_t value);
    uint32_t setting_get(uint16_t id);

  private:
    Serial::Serial _serial;

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
        std::variant<std::monostate, AresFrame::AresFrameSetting,
                     AresFrame::AresFrameAckErrorCode,
                     AresFrame::AresFrameFramingError>
            frame;
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

    // todo: event handler
};

#endif // ARES_ARES_LORA_SERIAL_HPP
