/**
 * @file command_response.cpp
 *
 * @brief
 *
 * @date 8/17/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame.hpp>
#include <ares-lora-serial/serial-driver/command_response.hpp>

void CommandResponse::clear() {
    error_codes.clear();
    response_values.clear();
}

py::object CommandResponse::specific_ret(const AresFrame::Setting &ret) {
    return py::cast(ret.value);
}

py::object CommandResponse::specific_ret(const AresFrame::Led &ret) {
    return py::cast(static_cast<uint8_t>(ret.state));
}

py::object CommandResponse::specific_ret(const AresFrame::Heartbeat &ret) {
    return py::cast(ret.ready);
}

static py::tuple decode_version(uint32_t version) {
    constexpr uint32_t mask = 0xFF;
    constexpr uint32_t minor_shift = 8;
    constexpr uint32_t major_shift = 16;

    uint32_t patch = version & mask;
    uint32_t minor = (version >> minor_shift) & mask;
    uint32_t major = (version >> major_shift) & mask;

    return py::make_tuple(major, minor, patch);
}

py::tuple CommandResponse::specific_ret(const AresFrame::Version &ret) {
    return py::make_tuple(decode_version(ret.app), decode_version(ret.ncs),
                          decode_version(ret.kernel));
}

py::object CommandResponse::specific_ret(const AresFrame::BleState &ret) {
    return py::cast(static_cast<uint8_t>(ret.state));
}

py::object
CommandResponse::specific_ret(const AresFrame::NodeConfigResponse &ret) {
    switch (ret.type) {
    case AresFrame::SAVE_FOLDER: {
        return py::cast(std::get<ares::DateTime>(ret.config).time_point());
    }
    case AresFrame::DURATION: {
        return py::cast(std::get<uint32_t>(ret.config));
    }
    case AresFrame::BANDWIDTH:
    case AresFrame::CENTER_FREQ:
    case AresFrame::REF_LEVEL: {
        return py::cast(std::get<double>(ret.config));
    }
    default: {
        return py::none();
    }
    }
}
