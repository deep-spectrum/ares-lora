/**
 * @file lora_config.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LORA_CONFIG_HPP
#define ARES_LORA_CONFIG_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct LoraConfig : Internal::FramePayloadBase {
    uint32_t frequency = 0;
    uint16_t preamble_length = 0;
    uint8_t bandwidth;
    uint8_t data_rate = 0;
    uint8_t coding_rate = 0;
    int8_t tx_power = 0;
    uint8_t cad_mode = 0;
    uint8_t cad_num_symbols = 0;
    uint8_t cad_det_peak = 0;
    uint8_t cad_det_min = 0;

    std::size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;

  private:
    static constexpr std::size_t _payload_size = 14;
};
} // namespace AresFrame

#endif // ARES_LORA_CONFIG_HPP
