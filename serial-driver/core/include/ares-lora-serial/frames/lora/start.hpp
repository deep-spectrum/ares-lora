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

#include <ares-lora-serial/frames/payload_base.hpp>
namespace AresFrame {
struct Start : Internal::FramePayloadBase {
    Start() = default;

    explicit Start(int64_t sec, uint64_t usec, uint16_t id, uint16_t packet_id,
                   bool broadcast, uint8_t seq_cnt)
        : sec(sec), usec(usec), id(id), packet_id(packet_id),
          broadcast(broadcast), seq_cnt(seq_cnt) {}

    int64_t sec = -1;
    uint64_t usec = 0;
    uint16_t id = 0;
    uint16_t packet_id = 0;
    bool broadcast = false;
    uint8_t seq_cnt = 0;

    std::size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr std::size_t _payload_size = 20;
};
} // namespace AresFrame

#endif // ARES_START_HPP
