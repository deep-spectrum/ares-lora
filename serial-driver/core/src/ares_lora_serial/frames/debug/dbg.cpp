/**
 * @file dbg.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/debug/dbg.hpp>
#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
void Dbg::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        throw AresFrameError("Invalid payload size received");
    }

    ares::deserialize(buffer, code);
}
} // namespace AresFrame
