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

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct Dbg : Internal::FramePayloadBase {
    int32_t code = 0;

    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 4;
};
} // namespace AresFrame

#endif // ARES_DBG_HPP
