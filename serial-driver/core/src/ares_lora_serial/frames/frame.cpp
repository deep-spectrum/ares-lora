/**
 * @file frame.cpp
 *
 * @brief
 *
 * @date 8/3/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame.hpp>
#include <ares/serialization.hpp>
#include <cassert>
#include <cstring>
#include <utility>

namespace AresFrame {
constexpr uint8_t header = '^';
constexpr uint8_t footer = '@';

constexpr size_t header_size = sizeof(header);
constexpr size_t len_size = sizeof(uint16_t);
constexpr size_t type_size = sizeof(AresFrameType);
constexpr size_t footer_size = sizeof(footer);
constexpr size_t frame_overhead =
    header_size + len_size + type_size + footer_size;
constexpr size_t head_overhead = header_size + len_size + type_size;

constexpr size_t header_offset = 0;
constexpr size_t len_offset = header_offset + header_size;
constexpr size_t type_offset = len_offset + len_size;
constexpr size_t payload_offset = type_offset + type_size;

constexpr size_t max_frame_size = 256;
constexpr size_t max_payload_size = 32;

static size_t footer_offset(size_t payload_size) {
    return payload_offset + payload_size;
}

static size_t retrieve_payload_len(const uint8_t *data) {
    uint16_t len;
    (void)memcpy(&len, &data[len_offset], sizeof(len));
    return len;
}

static AresFrameType get_frame_type(const Frame::TxTypes &v) {
    return std::visit(
        [](const auto &value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return DRIVER_STOP;
            } else {
                return value.frame_type;
            }
        },
        v);
}

Frame::Frame(const TxTypes &tx_payload)
    : _direction(TX), _type(get_frame_type(tx_payload)) {
    _tx_payload = tx_payload;
}

Frame::Frame(const std::vector<uint8_t> &buffer) {
    auto [start_index, length, bytes_left] = frame_present(buffer);

    _type = UNKNOWN;
    if (start_index >= 0) {
        parse(buffer, start_index);
        return;
    }

    throw AresFrameError("Not an Ares Frame");
}

Frame::Frame(const Frame &other) {
    _new_frame = other._new_frame;
    _type = other._type;
    _tx_payload = other._tx_payload;
    _rx_payload = other._rx_payload;
}

Frame &Frame::operator=(const Frame &other) {
    _new_frame = other._new_frame;
    _type = other._type;
    _tx_payload = other._tx_payload;
    _rx_payload = other._rx_payload;
    return *this;
}

std::tuple<ssize_t, ssize_t, ssize_t>
Frame::frame_present(const uint8_t *serial_data, size_t len,
                     bool error_no_footer) {
    return frame_present(std::vector(serial_data, serial_data + len),
                         error_no_footer);
}

std::tuple<ssize_t, ssize_t, ssize_t>
Frame::frame_present(const std::vector<uint8_t> &bytearray,
                     bool error_no_footer) {
    size_t len = bytearray.size();
    size_t header_idx, type_idx, frame_size, payload_len, footer_idx;

    for (size_t i = 0; i < len; i++) {
        if (bytearray[i] != header) {
            continue;
        }

        header_idx = i;

        type_idx = header_idx + type_offset;
        if (type_idx > len) {
            continue;
        }

        payload_len = retrieve_payload_len(&bytearray[header_idx]);

        footer_idx = header_idx + footer_offset(payload_len);
        if (footer_idx >= len && error_no_footer) {
            continue;
        }

        if (error_no_footer && bytearray[footer_idx] != footer) {
            continue;
        }

        frame_size = frame_overhead + payload_len;
        return {header_idx, frame_size, footer_idx - (len - header_idx) + 1};
    }

    return {-1, -1, -1};
}

void Frame::serialize(std::vector<uint8_t> &bytearray) {
    _direction = TX;
    bytearray.clear();
    _preprocess();
    uint16_t payload_size = _payload_size();
    std::vector<uint8_t> payload_v;

    ares::serialize(bytearray, header, payload_size, _type);

    std::visit(
        [&payload_v](auto &payload) {
            if constexpr (!std::is_same_v<std::decay_t<decltype(payload)>,
                                          std::monostate>) {
                payload.serialize(payload_v);
            }
        },
        _tx_payload);

    bytearray.insert(bytearray.end(), payload_v.begin(), payload_v.end());

    ares::serialize(bytearray, footer);
}

void Frame::parse(const uint8_t *serial_data, size_t start_index, size_t len) {
    parse(std::vector(serial_data, serial_data + len), start_index);
}

void Frame::parse(const std::vector<uint8_t> &bytearray, size_t start_index) {
    _direction = RX;
    uint16_t payload_size;
    uint8_t header_, type_;

    ares::deserialize(bytearray.data() + start_index, header_, payload_size,
                      type_);
    _type = static_cast<AresFrameType>(type_);

    auto rx_it = _rx_map.find(_type);
    if (rx_it == _rx_map.end()) {
        throw AresFrameError("Invalid RX type");
    }

    _rx_payload = rx_it->second();

    std::visit(
        [&bytearray, start_index, payload_size](auto &payload) {
            if constexpr (!std::is_same_v<std::decay_t<decltype(payload)>,
                                          std::monostate>) {
                payload.deserialize(bytearray.data() + start_index +
                                        payload_offset,
                                    payload_size);
            }
        },
        _rx_payload);
}

bool Frame::frame_available() const {
    if (_direction != TX) {
        return false;
    }

    return _new_frame;
}

size_t Frame::total_frames() const {
    size_t total_frames = 1;
    if (_direction != TX) {
        return 0;
    }

    std::visit(
        [&total_frames](const auto &payload) {
            if constexpr (!std::is_same_v<std::decay_t<decltype(payload)>,
                                          std::monostate>) {
                total_frames = payload.num_frames();
            }
        },
        _tx_payload);

    return total_frames;
}

AresFrameType Frame::type() const { return _type; }

Frame::RxTypes Frame::rx_payload() const { return _rx_payload; }

Frame::TxTypes Frame::tx_payload() const { return _tx_payload; }

void Frame::_preprocess() {
    std::visit(
        [this](auto &payload) {
            if constexpr (!std::is_same_v<std::decay_t<decltype(payload)>,
                                          std::monostate>) {
                payload.preprocess();
                _new_frame = payload.new_frame();
            }
        },
        _tx_payload);
}

uint16_t Frame::_payload_size() {
    size_t size = 0;

    std::visit(
        [&size](auto &payload) {
            if constexpr (!std::is_same_v<std::decay_t<decltype(payload)>,
                                          std::monostate>) {
                size = payload.payload_size();
            }
        },
        _tx_payload);

    return static_cast<uint16_t>(size);
}
} // namespace AresFrame
