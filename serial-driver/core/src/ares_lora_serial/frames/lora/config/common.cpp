/**
 * @file common.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/lora/config/common.hpp>
#include <ares/serialization.hpp>

namespace AresFrame::Internal {
void serialize_save_folder(const NodeConfigSaveFolder &config,
                           uint64_t &config_item) {
    ares::set_bitfield(config_item, 0, 6, config.second);
    ares::set_bitfield(config_item, 6, 6, config.minute);
    ares::set_bitfield(config_item, 12, 5, config.hour);
    ares::set_bitfield(config_item, 17, 5, config.day);
    ares::set_bitfield(config_item, 22, 4, config.month);
    ares::set_bitfield(config_item, 26, 14, config.year);
}

void deserialize_save_folder(NodeConfigSaveFolder &config,
                             uint64_t config_item) {
    ares::get_bitfield(config_item, 0, 6, config.second);
    ares::get_bitfield(config_item, 6, 6, config.minute);
    ares::get_bitfield(config_item, 12, 5, config.hour);
    ares::get_bitfield(config_item, 17, 5, config.day);
    ares::get_bitfield(config_item, 22, 4, config.month);
    ares::get_bitfield(config_item, 26, 14, config.year);
}
} // namespace AresFrame::Internal
