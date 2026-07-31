/**
 * @file poll.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_POLL_HPP
#define ARES_POLL_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct Poll : Internal::FramePayloadBase {
    uint16_t id = 0;

    size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 2;
};
} // namespace AresFrame

#endif // ARES_POLL_HPP
