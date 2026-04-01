/**
 * @file ares_frame.cpp
 *
 * @brief
 *
 * @date 3/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/ares_frame.hpp>
#include <ares-lora-serial/util.h>
#include <cstring>

constexpr uint8_t header = '^';
constexpr uint8_t footer = '@';

constexpr size_t header_size = sizeof(header);
constexpr size_t len_size = sizeof(uint16_t);
constexpr size_t type_size = sizeof(uint8_t);
constexpr size_t footer_size = sizeof(footer);
constexpr size_t frame_overhead =
    header_size + len_size + type_size + footer_size;
constexpr size_t head_overhead = header_size + len_size + type_size;

constexpr size_t header_offset = 0;
constexpr size_t len_offset = header_offset + header_size;
constexpr size_t type_offset = len_offset + len_size;
constexpr size_t payload_offset = type_offset + type_size;

static size_t footer_offset(size_t payload_size) {
    return payload_offset + payload_size;
}

static size_t retrieve_payload_len(const uint8_t *data) {
    uint16_t len;
    (void)memcpy(&len, &data[len_offset], sizeof(len));
    return len;
}

AresFrame::AresFrame(AresFrameType type, AresFrameTxTypes payload)
    : _direction(TX), _type(type) {
    _tx_payload = payload;
}

AresFrame::AresFrame(const std::vector<uint8_t> &bytearray) : _direction(RX) {
    auto [start_index, length, bytes_left] = frame_present(bytearray);

    _type = SETTING;
    if (start_index >= 0) {
        parse(bytearray, start_index);
        return;
    }
    throw AresFrameError("Not an Ares frame");
}

std::tuple<ssize_t, ssize_t, ssize_t>
AresFrame::frame_present(const uint8_t *serial_data, size_t len,
                         bool error_no_footer) {
    return frame_present(std::vector(serial_data, serial_data + len),
                         error_no_footer);
}

std::tuple<ssize_t, ssize_t, ssize_t>
AresFrame::frame_present(const std::vector<uint8_t> &bytearray,
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
            // cannot extract length
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

void AresFrame::serialize(std::vector<uint8_t> &bytearray) {
    _direction = TX;
    bytearray.clear();

    bytearray.resize(head_overhead);
    bytearray[header_offset] = header;

    uint16_t payload_size = _payload_size();
    (void)memcpy(&bytearray[len_offset], &payload_size, sizeof(payload_size));
    bytearray[type_offset] = static_cast<uint8_t>(_type);

    switch (_type) {
    case SETTING: {
        _serialize_setting(std::get<AresFrameSetting>(_tx_payload), bytearray);
        break;
    }
    case START: {
        _serialize_start(std::get<AresFrameStart>(_tx_payload), bytearray);
        break;
    }
    case LORA_CONFIG: {
        _serialize_lora_config(std::get<AresFrameLoraConfig>(_tx_payload),
                               bytearray);
        break;
    }
    default: {
        throw AresFrameError("Invalid type for TX");
    }
    }

    bytearray.emplace_back(footer);
}

void AresFrame::parse(const uint8_t *serial_data, size_t start_index,
                      size_t len) {
    parse(std::vector(serial_data, serial_data + len), start_index);
}

static size_t extract_payload_len(const uint8_t *buf) {
    uint16_t payload_len;
    (void)memcpy(&payload_len, &buf[len_offset], sizeof(payload_len));
    return payload_len;
}

void AresFrame::parse(const std::vector<uint8_t> &bytearray,
                      size_t start_index) {
    _direction = RX;
    size_t payload_len = extract_payload_len(&bytearray[start_index]);
    _type = static_cast<AresFrameType>(bytearray[start_index + type_offset]);

    switch (_type) {
    case SETTING: {
        _deserialize_setting(&bytearray[start_index + payload_offset],
                             payload_len);
        break;
    }
    case START: {
        _deserialize_start(&bytearray[start_index + payload_offset],
                           payload_len);
        break;
    }
    case ACK: {
        _deserialize_ack(&bytearray[start_index + payload_offset], payload_len);
        break;
    }
    case FRAMING_ERROR: {
        _deserialize_framing_error(&bytearray[start_index + payload_offset],
                                   payload_len);
        break;
    }
    default: {
        throw AresFrameError("Invalid RX type");
    }
    }
}

AresFrame::AresFrameDecoded AresFrame::get_parsed_frame() const {
    AresFrameDecoded decoded = {.type = _type, .payload = _rx_payload};
    return decoded;
}

uint16_t AresFrame::_payload_size() const {
    uint16_t ret = 0;

    switch (_type) {
    case SETTING: {
        auto [set, setting_id, value] = std::get<AresFrameSetting>(_tx_payload);
        ret = sizeof(setting_id);
        if (set) {
            ret += sizeof(value);
        }
        break;
    }
    case START: {
        ret = sizeof(AresFrameStart::sec) + sizeof(AresFrameStart::nsec) +
              sizeof(AresFrameStart::id) + sizeof(AresFrameStart::broadcast) +
              sizeof(AresFrameStart::seq_cnt) +
              sizeof(AresFrameStart::packet_id);
        break;
    }
    case LORA_CONFIG: {
        ret = sizeof(AresFrameLoraConfig::frequency) +
              sizeof(AresFrameLoraConfig::preamble_length) +
              sizeof(AresFrameLoraConfig::bandwidth) +
              sizeof(AresFrameLoraConfig::data_rate) +
              sizeof(AresFrameLoraConfig::coding_rate) +
              sizeof(AresFrameLoraConfig::tx_power);
        break;
    }
    default: {
        throw AresFrameError("Invalid TX type");
    }
    }

    return ret;
}

#define SERIALIZE(field)                                                       \
    do {                                                                       \
        const auto *val_ = reinterpret_cast<const uint8_t *>(&payload.field);  \
        buffer.insert(buffer.end(), val_, val_ + sizeof(payload.field));       \
    } while (false)

void AresFrame::_serialize_setting(const AresFrameSetting &payload,
                                   std::vector<uint8_t> &buffer) {
    SERIALIZE(setting_id);

    if (payload.set) {
        SERIALIZE(value);
    }
}

void AresFrame::_serialize_start(const AresFrameStart &payload,
                                 std::vector<uint8_t> &buffer) {
    SERIALIZE(sec);
    SERIALIZE(nsec);
    SERIALIZE(id);
    SERIALIZE(broadcast);
    SERIALIZE(seq_cnt);
    SERIALIZE(packet_id);
}

void AresFrame::_serialize_lora_config(const AresFrameLoraConfig &payload,
                                       std::vector<uint8_t> &buffer) {
    SERIALIZE(frequency);
    SERIALIZE(preamble_length);
    SERIALIZE(bandwidth);
    SERIALIZE(data_rate);
    SERIALIZE(coding_rate);
    SERIALIZE(tx_power);
}

#define DESERIALIZE_INIT(class_)                                               \
    class_ val_;                                                               \
    size_t offset_ = 0
#define DESERIALIZE(field)                                                     \
    memcpy(&val_.field, buf + offset_, sizeof(val_.field));                    \
    offset_ += sizeof(val_.field)
#define DESERIALIZE_FINALIZE() _rx_payload = val_

void AresFrame::_deserialize_setting(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(AresFrameSetting);
    DESERIALIZE(setting_id);
    DESERIALIZE(value);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_start(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(AresFrameStart);
    DESERIALIZE(sec);
    DESERIALIZE(nsec);
    DESERIALIZE(id);
    DESERIALIZE(broadcast);
    DESERIALIZE(seq_cnt);
    DESERIALIZE(packet_id);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_ack(const uint8_t *buf, size_t len) {
    AresFrameAckErrorCode code;
    memcpy(&code, buf, len);
    _rx_payload = code;
}

void AresFrame::_deserialize_framing_error(const uint8_t *buf, size_t len) {
    AresFrameFramingError error;
    uint32_t mem;
    memcpy(&mem, buf, len);
    error = static_cast<AresFrameFramingError>(mem);
    _rx_payload = error;
}
