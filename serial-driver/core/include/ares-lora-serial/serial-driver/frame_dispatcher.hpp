/**
 * @file frame_dispatcher.hpp
 *
 * @brief
 *
 * @date 8/11/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_FRAME_DISPATCHER_HPP
#define ARES_FRAME_DISPATCHER_HPP

#include <ares-lora-serial/serial-driver/command_response.hpp>
#include <ares-lora-serial/serial-driver/serial_driver.hpp>
#include <ares/pyutil.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <vector>

namespace py = pybind11;

/**
 * @class FrameDispatcher
 * Helper class for dispatching frames and listening for their responses.
 */
class FrameDispatcher {
  public:
    /**
     * KWArgs constructor.
     * @param[in] serial The serial object.
     * @param[in] cmd_specific_supported Flag indicating if a command specific
     * response is supported.
     * @param[in] kwargs The Python keyword arguments.
     */
    explicit FrameDispatcher(AresSerial &serial, bool cmd_specific_supported,
                             const py::kwargs &kwargs)
        : _serial(serial), command_specific_supported(cmd_specific_supported) {
        ares::from_kwargs(kwargs, SP(response_timeout), SP(ack_timeout),
                          SP(retries), SP(broadcast), SP(destination));
    }

    /**
     * Response constructor.
     * @param serial The serial object.
     * @param response_timeout The amount of time to wait for a response.
     */
    explicit FrameDispatcher(AresSerial &serial,
                             std::chrono::milliseconds response_timeout)
        : _serial(serial), response_timeout(response_timeout),
          lora_fields_already_set(true) {}

    /**
     * Send a frame over serial and retrieve its response.
     * @tparam T The Tx payload type in the variant.
     * @param[in] payload The frame payload to send.
     * @return The response from the firmware.
     */
    template <typename T> CommandResponse send_frame(T &payload) {
        AresFrame::Frame::TxTypes fucking_bullshit = payload;
        return send_frame(fucking_bullshit);
    }

    /**
     * Send a frame over serial and retrieve its response.
     * @param[in] payload The frame payload to send.
     * @return The response from the firmware.
     */
    CommandResponse send_frame(AresFrame::Frame::TxTypes &payload);

    /**
     * Send a frame over serial while the GIL is released.
     * @tparam T The Tx payload type in the variant.
     * @param[in] payload The frame payload to send.
     * @param[in,out] responses The response(s) from firmware.
     */
    template <typename T>
    void
    send_frame_released(T &payload,
                        std::vector<AresSerial::FrameResponse> &responses) {
        AresFrame::Frame::TxTypes fucking_bullshit = payload;
        send_frame_released(fucking_bullshit, responses);
    }

    /**
     * Send a frame over serial while the GIL is released.
     * @param[in] payload The frame payload to send.
     * @param[in,out] responses The response(s) from firmware.
     */
    void send_frame_released(AresFrame::Frame::TxTypes &payload,
                             std::vector<AresSerial::FrameResponse> &responses);

    /**
     * Get the frame type that was dispatched.
     * @return The frame type dispatched.
     */
    [[nodiscard]] AresFrame::AresFrameType type_dispatched() const;

    /**
     * Check if the message is a broadcast message.
     * @return @p true if the message is a broadcast message, @p false
     * otherwise.
     */
    [[nodiscard]] bool broadcast_msg() const;

  private:
    AresSerial &_serial;
    std::chrono::milliseconds response_timeout = 2s;
    std::chrono::milliseconds ack_timeout = 5s;
    uint32_t retries = 0;
    bool broadcast = false;
    uint16_t destination = 0;
    const bool lora_fields_already_set = false;

    bool is_lora_payload = false;
    AresFrame::AresFrameType _type_dispatched = AresFrame::UNKNOWN;
    bool broadcast_supported = false;
    bool lora_response_supported = false;
    bool command_specific_supported = false;

    std::vector<AresFrame::Frame::AckTypes> _lora_responses;
    CommandResponse _response;

    void _send_frame(AresFrame::Frame &frame,
                     const std::chrono::milliseconds &timeout,
                     std::vector<AresSerial::FrameResponse> &responses);
    void
    _send_frame_released(AresFrame::Frame &frame,
                         const std::chrono::milliseconds &timeout,
                         std::vector<AresSerial::FrameResponse> &responses);
    void _send_frame_released(const std::vector<uint8_t> &buf) const;

    void _send_frame_normal_released(
        AresFrame::Frame &frame, const std::chrono::milliseconds &timeout,
        std::vector<AresSerial::FrameResponse> &responses) const;

    void _send_lora_expecting_response_released(
        AresFrame::Frame &frame, const std::chrono::milliseconds &timeout,
        std::vector<AresSerial::FrameResponse> &responses);
    bool _wait_lora_response(AresFrame::Frame &frame, bool final_try);

    [[nodiscard]] AresSerial::FrameResponse
    _wait_response(const std::chrono::milliseconds &timeout) const;
    [[nodiscard]] AresSerial::FrameResponse
    _wait_response_timeout(const std::chrono::milliseconds &timeout) const;
    [[nodiscard]] AresSerial::FrameResponse _wait_response_forever() const;

    void _verify_responses(
        const std::vector<AresSerial::FrameResponse> &responses) const;
    [[nodiscard]] bool
    _verify_response(const AresSerial::FrameResponse &response) const;

    static void _handle_bad_frame(const AresSerial::FrameResponse &response);

    void
    _process_responses(const std::vector<AresSerial::FrameResponse> &responses);
};

#endif // ARES_FRAME_DISPATCHER_HPP
