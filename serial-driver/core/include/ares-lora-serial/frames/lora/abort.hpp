/**
 * @file abort.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_ABORT_HPP
#define ARES_ABORT_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct Abort : Internal::FramePayloadBase {
    /**
     * On transmission, indicate if the message should be broadcasted. On
     * reception, indicates if the message was broadcasted.
     */
    bool broadcast = false;

    /**
     * On transmission, the node id to send the abort message to if being
     * directed. On reception, the node id the message came from.
     */
    uint16_t id = 0;

    size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 3;
};
} // namespace AresFrame

#endif // ARES_ABORT_HPP
