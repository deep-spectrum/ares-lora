/**
 * @file heartbeat.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_HEARTBEAT_HPP
#define ARES_HEARTBEAT_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct Heartbeat : Internal::FramePayloadBase {
    Heartbeat() = default;

    explicit Heartbeat(bool ready, uint16_t id) : ready(ready), id(id) {}

    bool ready = false;
    uint16_t id = 0;

    std::size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr std::size_t _payload_size = 3;
};
} // namespace AresFrame

#endif // ARES_HEARTBEAT_HPP
