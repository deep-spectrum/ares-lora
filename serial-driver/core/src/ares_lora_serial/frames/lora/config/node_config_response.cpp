/**
 * @file node_config_response.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/config/node_config_response.hpp>
#include <ares/serialization.hpp>
#include <cassert>

namespace AresFrame {
size_t NodeConfigResponse::payload_size() { return _payload_size; }

void NodeConfigResponse::serialize(std::vector<uint8_t> &buffer) {
    uint64_t config_field = 0;

    switch (type) {
    case SAVE_FOLDER: {
        Internal::serialize_save_folder(std::get<NodeConfigSaveFolder>(config),
                                        config_field);
        break;
    }
    case DURATION: {
        config_field = static_cast<uint64_t>(std::get<uint32_t>(config));
        break;
    }
    case BANDWIDTH:
    case CENTER_FREQ:
    case REF_LEVEL: {
        (void)std::memcpy(&config_field, &std::get<double>(config),
                          sizeof(config_field));
        break;
    }
    default: {
        throw AresFrameError("Unknown configuration type");
        break;
    }
    }

    ares::serialize(buffer, id, type, config_field);
    assert(buffer.size() == _payload_size);
}

void NodeConfigResponse::deserialize(const uint8_t *buffer, std::size_t len) {
    uint64_t config_field = 0;

    if (len != _payload_size) {
        throw AresFrameError("Invalid payload size received");
    }

    ares::deserialize(buffer, id, type, config_field);

    switch (type) {
    case SAVE_FOLDER: {
        NodeConfigSaveFolder ret;
        Internal::deserialize_save_folder(ret, config_field);
        config = ret;
        break;
    }
    case DURATION: {
        config = static_cast<uint32_t>(config_field);
        break;
    }
    case BANDWIDTH:
    case CENTER_FREQ:
    case REF_LEVEL: {
        double ret;
        (void)std::memcpy(&ret, &config_field, sizeof(ret));
        config = ret;
        break;
    }
    default: {
        throw AresFrameError("Unknown configuration type");
        break;
    }
    }
}

bool NodeConfigResponse::operator==(const NodeConfigResponse &rhs) const {
    return type == rhs.type && id == rhs.id;
}
} // namespace AresFrame
