/**
 * @file ble_config.cpp
 *
 * @brief
 *
 * @date 11/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/ble/ble_config.hpp>
#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
void BleNodeConfiguration::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len <= _overhead) {
        throw AresFrameError("Invalid payload size received");
    }

    ares::deserialize(buffer, type);

    switch (type) {
    case BANDWIDTH:
    case CENTER_FREQUENCY:
    case REFERENCE_LEVEL: {
        double value;
        ares::deserialize(buffer + _overhead, value);
        config = value;
        break;
    }
    case DURATION: {
        uint32_t value;
        ares::deserialize(buffer + _overhead, value);
        config = value;
        break;
    }
    case DESCRIPTION: {
        config =
            std::string(reinterpret_cast<const char *>(buffer + _overhead));
        break;
    }
    default: {
        throw AresFrameError("Invalid configuration type");
    }
    }
}

void BleNodeConfiguration::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, type);
    switch (type) {
    case BANDWIDTH:
    case CENTER_FREQUENCY:
    case REFERENCE_LEVEL: {
        ares::serialize(buffer, as_double());
        break;
    }
    case DURATION: {
        ares::serialize(buffer, as_uint32());
        break;
    }
    case DESCRIPTION: {
        std::string s = as_str();
        buffer.insert(buffer.end(), s.begin(), s.end());
        break;
    }
    default: {
        throw AresFrameError("Invalid configuration type");
    }
    }
    assert(buffer.size() == payload_size());
}

std::size_t BleNodeConfiguration::payload_size() {
    size_t ret = sizeof(type);

    switch (type) {
    case BANDWIDTH:
    case CENTER_FREQUENCY:
    case REFERENCE_LEVEL: {
        ret += sizeof(double);
        break;
    }
    case DURATION: {
        ret += sizeof(uint32_t);
        break;
    }
    case DESCRIPTION: {
        ret += as_str().length() + 1;
        break;
    }
    default: {
        throw AresFrameError("Invalid configuration type");
    }
    }

    return ret;
}

double BleNodeConfiguration::as_double() const {
    return std::get<double>(config);
}

uint32_t BleNodeConfiguration::as_uint32() const {
    return std::get<uint32_t>(config);
}

std::string BleNodeConfiguration::as_str() const {
    return std::get<std::string>(config);
}
} // namespace AresFrame
