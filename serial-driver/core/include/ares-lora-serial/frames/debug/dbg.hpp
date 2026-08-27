/**
 * @file dbg.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_DBG_HPP
#define ARES_DBG_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {

/**
 * @struct Dbg
 *
 * Data for AresFrame::DBG frames.
 */
struct Dbg : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = DBG;

    /**
     * Error code from firmware.
     */
    int32_t code = 0;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 4;
};
} // namespace AresFrame

#endif // ARES_DBG_HPP
