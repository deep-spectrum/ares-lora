/**
 * @file lora_config.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/mcu/lora_config.hpp>
#include <ares/serialization.hpp>
#include <cassert>

namespace AresFrame {
std::size_t LoraConfig::payload_size() { return _payload_size; }

void LoraConfig::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, frequency, preamble_length, bandwidth, data_rate,
                    coding_rate, tx_power, cad_mode, cad_num_symbols,
                    cad_det_peak, cad_det_min);
    assert(buffer.size() == _payload_size);
}
} // namespace AresFrame
