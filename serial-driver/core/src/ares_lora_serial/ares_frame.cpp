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
#include <ares/util.h>
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

AresFrame::AresFrame() : _direction(UNSPECIFIED), _type(UNKNOWN) {}

AresFrame::AresFrame(const AresFrame &other) = default;

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
    _preprocess_serialize();

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
    case LED: {
        _serialize_led(std::get<AresFrameLed>(_tx_payload), bytearray);
        break;
    }
    case HEARTBEAT: {
        _serialize_heartbeat(std::get<AresFrameHeartbeat>(_tx_payload),
                             bytearray);
        break;
    }
    case CLAIM: {
        _serialize_claim(std::get<AresFrameClaim>(_tx_payload), bytearray);
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
    case LED: {
        _deserialize_led(&bytearray[start_index + payload_offset], payload_len);
        break;
    }
    case HEARTBEAT: {
        _deserialize_heartbeat(&bytearray[start_index + payload_offset],
                               payload_len);
        break;
    }
    case CLAIM: {
        _deserialize_claim(&bytearray[start_index + payload_offset],
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

bool AresFrame::frame_available() const { return _new_frame; }

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
    case LED: {
        ret = sizeof(AresFrameLed::state);
        break;
    }
    case CLAIM: {
        ret = sizeof(AresFrameClaim::id);
        break;
    }
    case HEARTBEAT: {
        ret = sizeof(AresFrameHeartbeat::tx_cnt) +
              sizeof(AresFrameHeartbeat::id) +
              sizeof(uint8_t); // flags are bit-packed
        break;
    }
    default: {
        throw AresFrameError("Invalid TX type");
    }
    }

    return ret;
}

void AresFrame::_preprocess_serialize() {
    switch (_type) {
    case LOG: {
        AresFrameLog payload = std::get<AresFrameLog>(_tx_payload);
        _preprocess_log(payload);
        _tx_payload = payload;
        _new_frame = payload._preprocessed;
        break;
    }
    default: {
        // doesn't need to be split into multiple frames
        _new_frame = false;
        break;
    }
    }
}

void AresFrame::_preprocess_log(AresFrameLog &payload) {
    if (payload._preprocessed) {
        payload._idx++;
        payload._part++;
        payload._preprocessed = payload._idx < payload._msg_split.size();
        return;
    }

    if (payload.msg.empty()) {
        throw AresFrameError("Log message empty");
    }

    size_t max_msg_size = max_frame_size - payload._overhead;
    size_t num_substr =
        (payload.msg.size() + (max_msg_size - 1)) / max_msg_size;

    if (num_substr > static_cast<size_t>(UINT8_MAX)) {
        throw AresFrameError("Log message too long");
    }

    payload._msg_split.reserve(num_substr);
    std::string_view content = payload.msg;
    size_t i = 0;

    do {
        payload._msg_split.emplace_back(content.substr(i, max_msg_size));
        i += payload._msg_split.back().size();
    } while (i < payload.msg.size());

    payload._part = 1;
    payload._idx = 0;
    payload._num_parts = payload._msg_split.size();

    payload._preprocessed = true;
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

void AresFrame::_serialize_led(const AresFrameLed &payload,
                               std::vector<uint8_t> &buffer) {
    SERIALIZE(state);
}

void AresFrame::_serialize_heartbeat(const AresFrameHeartbeat &payload,
                                     std::vector<uint8_t> &buffer) {
    uint8_t flags = 0;
    if (payload.ready) {
        flags |= 1;
    }
    if (payload.broadcast) {
        flags |= 2;
    }

    buffer.emplace_back(flags);
    SERIALIZE(tx_cnt);
    SERIALIZE(id);
}

void AresFrame::_serialize_claim(const AresFrameClaim &payload,
                                 std::vector<uint8_t> &buffer) {
    SERIALIZE(id);
}

void AresFrame::_serialize_log(const AresFrameLog &payload,
                               std::vector<uint8_t> &buffer) {
    SERIALIZE(broadcast);
    SERIALIZE(id);
    SERIALIZE(tx_cnt);
    SERIALIZE(_part);
    SERIALIZE(_num_parts);
    buffer.insert(buffer.end(), payload._msg_split[payload._idx].begin(),
                  payload._msg_split[payload._idx].end());
}

#define Z_DESERIALIZE_INIT_DEFAULT(class_)                                     \
    class_ val_;                                                               \
    size_t offset_ = 0

#define Z_DESERIALIZE_INIT_OFFSET(class_, offset_val_)                         \
    class_ val_;                                                               \
    size_t offset_ = (offset_val_)

#define DESERIALIZE_INIT(class_, start_offset...)                              \
    COND_CODE_0(IS_EMPTY(start_offset),                                        \
                (Z_DESERIALIZE_INIT_OFFSET(class_, start_offset)),             \
                (Z_DESERIALIZE_INIT_DEFAULT(class_)))

#define DESERIALIZE(field)                                                     \
    memcpy(&val_.field, buf + offset_, sizeof(val_.field));                    \
    offset_ += sizeof(val_.field)

#define DESERIALIZE_STR(field, len)                                            \
    val_.field = std::string(buf + offset_, buf + offset_ + (len));            \
    offset_ += (len)

#define DESERIALIZE_SET(field, val)         val_.field = val

#define DESERIALIZE_SET_ADVANCE(num_bytes_) offset_ += (num_bytes_)

#define DESERIALIZE_FINALIZE()              _rx_payload = val_

void AresFrame::_deserialize_setting(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(AresFrameSetting);
    DESERIALIZE(setting_id);
    DESERIALIZE(value);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_led(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(AresFrameLed);
    DESERIALIZE(state);
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

void AresFrame::_deserialize_heartbeat(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(AresFrameHeartbeat, 1);
    DESERIALIZE_SET(ready, (buf[0] & 1) != 0);
    DESERIALIZE_SET(broadcast, (buf[0] & 2) != 0);
    // No need to advance here. Offset is already set to 1 byte...
    DESERIALIZE(tx_cnt);
    DESERIALIZE(id);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_claim(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(AresFrameClaim);
    DESERIALIZE(id);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_log(const uint8_t *buf, size_t len) {
    DESERIALIZE_INIT(AresFrameLog);
    DESERIALIZE(broadcast);
    DESERIALIZE(id);
    DESERIALIZE(tx_cnt);
    DESERIALIZE(part);
    DESERIALIZE(num_parts);
    DESERIALIZE_STR(msg, len - AresFrameLog::_overhead);
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
