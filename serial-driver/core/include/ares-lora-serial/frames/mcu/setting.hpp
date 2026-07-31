/**
 * @file setting.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SETTING_HPP
#define ARES_SETTING_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct Setting : Internal::FramePayloadBase {
    /**
     * Flag indicating if the set or get frame should be used.
     */
    bool set = false;

    /**
     * The setting ID.
     */
    uint16_t setting_id = 0;

    /**
     * The value of the frame.
     */
    uint32_t value = 0;

    [[nodiscard]] std::size_t payload_size() override;

    void preprocess() override;

    void serialize(std::vector<uint8_t> &buffer) override;

    void deserialize(const uint8_t *payload, std::size_t len) override;

    bool new_frame() override;

  private:
    static constexpr std::size_t set_payload_size = 6;
    static constexpr std::size_t get_payload_size = 2;
};
} // namespace AresFrame

#endif // ARES_SETTING_HPP
