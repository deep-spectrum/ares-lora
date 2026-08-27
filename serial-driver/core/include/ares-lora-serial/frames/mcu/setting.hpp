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

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct Setting
 *
 * Data for AresFrame::SETTING frames.
 */
struct Setting : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = SETTING;

    /**
     * Constructor.
     */
    Setting() = default;

    /**
     * Set setting constructor.
     * @param setting_id See Setting::setting_id.
     * @param value See Setting::value.
     */
    explicit Setting(uint16_t setting_id, uint32_t value)
        : set(true), setting_id(setting_id), value(value) {}

    /**
     * Get setting constructor.
     * @param id See Setting::id.
     */
    explicit Setting(uint16_t id) : setting_id(id) {}

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

    /**
     * Payload size.
     * @return The payload size.
     */
    [[nodiscard]] std::size_t payload_size() override;

    /**
     * Encode into a buffer.
     * @param buffer The buffer to place data into.
     */
    void serialize(std::vector<uint8_t> &buffer) override;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr std::size_t set_payload_size = 6;
    static constexpr std::size_t get_payload_size = 2;
};
} // namespace AresFrame

#endif // ARES_SETTING_HPP
