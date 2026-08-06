/**
 * @file reboot.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/mcu/reboot.hpp>
#include <ares/serialization.hpp>
#include <cassert>

namespace AresFrame {
size_t Reboot::payload_size() { return _payload_size; }

void Reboot::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, delay);
    assert(buffer.size() == _payload_size);
}
} // namespace AresFrame
