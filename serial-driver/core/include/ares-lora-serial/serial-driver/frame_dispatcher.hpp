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

#include <ares-lora-serial/serial-driver/serial_driver.hpp>
#include <ares/pyutil.hpp>
#include <pybind11/pybind11.h>
#include <vector>

namespace py = pybind11;

class FrameDispatcher {
  public:
    FrameDispatcher(AresSerial &serial, const py::kwargs &kwargs)
        : _serial(serial), _kwargs(kwargs) {
        ares::from_kwargs(kwargs, SP(response_timeout), SP(ack_timeout),
                          SP(retries), SP(broadcast), SP(destination));
    }

    py::dict send_frame(AresFrame::Frame::TxTypes &payload);

  private:
    AresSerial &_serial;
    std::chrono::milliseconds response_timeout = 2s;
    std::chrono::milliseconds ack_timeout = 5s;
    uint32_t retries = 0;
    bool broadcast = false;
    uint16_t destination = 0;

    bool is_lora_payload = false;
    AresFrame::AresFrameType type_dispatched = AresFrame::UNKNOWN;
    bool broadcast_supported = false;
    bool lora_response_supported = false;

    const py::kwargs &_kwargs;

    void _send_frame(AresFrame::Frame &frame,
                     const std::chrono::milliseconds &timeout,
                     std::vector<AresSerial::FrameResponse> &responses) const;
    void _send_frame_released(
        AresFrame::Frame &frame, const std::chrono::milliseconds &timeout,
        std::vector<AresSerial::FrameResponse> &responses) const;
    void _send_frame_released(const std::vector<uint8_t> &buf) const;
    [[nodiscard]] AresSerial::FrameResponse
    _wait_response(const std::chrono::milliseconds &timeout) const;
    [[nodiscard]] AresSerial::FrameResponse
    _wait_response_timeout(const std::chrono::milliseconds &timeout) const;
    [[nodiscard]] AresSerial::FrameResponse _wait_response_forever() const;

    static void _handle_bad_frame(const AresSerial::FrameResponse &response);
};

#endif // ARES_FRAME_DISPATCHER_HPP
