/**
 * @file packet_rx.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_PACKET_RX_HPP
#define ARES_PACKET_RX_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {

/**
 * @struct PktRx
 *
 * Data for AresFrame::PKT_RX frames.
 */
struct PktRx : Internal::FramePayloadBase {
    /**
     * Packet ID.
     */
    uint16_t packet_id = 0;
    /**
     * Packet source ID.
     */
    uint16_t src_id = 0;

    /**
     * Sequence counter.
     */
    uint8_t seq_cnt = 0;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 5;
};
} // namespace AresFrame

#endif // ARES_PACKET_RX_HPP
