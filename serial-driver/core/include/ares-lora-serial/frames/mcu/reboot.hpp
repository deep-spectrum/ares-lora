/**
 * @file reboot.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_REBOOT_HPP
#define ARES_REBOOT_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct Reboot : Internal::FramePayloadBase {
    /**
     * The delay in seconds before the reboot occurs.
     */
    uint8_t delay = 5;

    size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;

  private:
    static constexpr size_t _payload_size = 1;
};
} // namespace AresFrame

#endif // ARES_REBOOT_HPP
