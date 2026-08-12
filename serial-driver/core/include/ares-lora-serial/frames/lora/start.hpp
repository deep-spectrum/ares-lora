/**
 * @file start.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_START_HPP
#define ARES_START_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/lora_ack.hpp>
#include <ares-lora-serial/frames/lora/lora_base.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct Start
 *
 * Data for AresFrame::START frames.
 */
struct Start : Internal::FramePayloadBase, Internal::LoraBase {
    static constexpr AresFrameType frame_type = START;
    using response_type = LoraAck;

    /**
     * Constructor.
     */
    Start() = default;

    /**
     * Constructor.
     * @param sec See Start::sec.
     * @param usec See Start::usec.
     * @param id See Start::id.
     * @param packet_id See Start::packet_id.
     * @param broadcast See Start::broadcast.
     * @param seq_cnt See Start::seq_cnt.
     */
    explicit Start(int64_t sec, uint64_t usec, uint16_t id, uint16_t packet_id,
                   bool broadcast, uint8_t seq_cnt)
        : sec(sec), usec(usec), id(id), packet_id(packet_id),
          broadcast(broadcast), seq_cnt(seq_cnt) {}

    /**
     * Seconds part for start time.
     */
    int64_t sec = -1;

    /**
     * Microseconds part for start time.
     */
    uint64_t usec = 0;

    /**
     * The destination ID on transmissions. The source ID on reception.
     */
    uint16_t id = 0;

    /**
     * Packet ID of the received packet. Ignored on transmissions.
     */
    uint16_t packet_id = 0;

    /**
     * On transmission, tells firmware to use a broadcast packet. On reception,
     * indicates if received packet was a broadcast.
     */
    bool broadcast = false;

    /**
     * The packet sequence count. Ignored on transmission.
     */
    uint8_t seq_cnt = 0;

    /**
     * Payload size.
     * @return The payload size.
     */
    std::size_t payload_size() override;

    /**
     * Encode into a buffer.
     * @param buffer The buffer to place data into.
     */
    void serialize(std::vector<uint8_t> &buffer) override;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr std::size_t _payload_size = 22;
};
} // namespace AresFrame

#endif // ARES_START_HPP
