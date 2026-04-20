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
#include <utility>

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

AresFrame::AresFrame(AresFrameType type, TxTypes payload)
    : _direction(TX), _type(type) {
    _tx_payload = std::move(payload);
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
        _serialize_setting(std::get<Setting>(_tx_payload), bytearray);
        break;
    }
    case START: {
        _serialize_start(std::get<Start>(_tx_payload), bytearray);
        break;
    }
    case LORA_CONFIG: {
        _serialize_lora_config(std::get<LoraConfig>(_tx_payload), bytearray);
        break;
    }
    case LED: {
        _serialize_led(std::get<Led>(_tx_payload), bytearray);
        break;
    }
    case HEARTBEAT: {
        _serialize_heartbeat(std::get<Heartbeat>(_tx_payload), bytearray);
        break;
    }
    case CLAIM: {
        _serialize_claim(std::get<Claim>(_tx_payload), bytearray);
        break;
    }
    case LOG: {
        _serialize_log(std::get<Log>(_tx_payload), bytearray);
        break;
    }
    case VERSION: {
        _serialize_version(std::get<Version>(_tx_payload), bytearray);
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
    case LOG: {
        _deserialize_log(&bytearray[start_index + payload_offset], payload_len);
        break;
    }
    case LOG_ACK: {
        _deserialize_log_ack(&bytearray[start_index + payload_offset],
                             payload_len);
        break;
    }
    case VERSION: {
        _deserialize_version(&bytearray[start_index + payload_offset],
                             payload_len);
        break;
    }
    case DBG: {
        _deserialize_dbg(&bytearray[start_index + payload_offset], payload_len);
        break;
    }
    default: {
        throw AresFrameError("Invalid RX type");
    }
    }
}

AresFrame::Decoded AresFrame::get_parsed_frame() const {
    Decoded decoded = {.type = _type, .payload = _rx_payload};
    return decoded;
}

bool AresFrame::frame_available() const { return _new_frame; }

size_t AresFrame::total_frames() const {
    if (_direction != TX) {
        return 0;
    }

    size_t ret = 1;

    switch (_type) {
    case LOG: {
        ret = std::get<Log>(_tx_payload)._num_parts;
        break;
    }
    default: {
        // nop
        break;
    }
    }

    return ret;
}

uint16_t AresFrame::_payload_size() const {
    uint16_t ret = 0;

    switch (_type) {
    case SETTING: {
        auto [set, setting_id, value] = std::get<Setting>(_tx_payload);
        ret = sizeof(setting_id);
        if (set) {
            ret += sizeof(value);
        }
        break;
    }
    case START: {
        ret = sizeof(Start::sec) + sizeof(Start::nsec) + sizeof(Start::id) +
              sizeof(Start::broadcast) + sizeof(Start::seq_cnt) +
              sizeof(Start::packet_id);
        break;
    }
    case LORA_CONFIG: {
        ret = sizeof(LoraConfig::frequency) +
              sizeof(LoraConfig::preamble_length) +
              sizeof(LoraConfig::bandwidth) + sizeof(LoraConfig::data_rate) +
              sizeof(LoraConfig::coding_rate) + sizeof(LoraConfig::tx_power);
        break;
    }
    case LED: {
        ret = sizeof(Led::state);
        break;
    }
    case CLAIM: {
        ret = sizeof(Claim::id);
        break;
    }
    case HEARTBEAT: {
        ret = sizeof(Heartbeat::tx_cnt) + sizeof(Heartbeat::id) +
              sizeof(uint8_t); // flags are bit-packed
        break;
    }
    case LOG: {
        Log payload = std::get<Log>(_tx_payload);
        ret = Log::_overhead + payload._msg_split[payload._idx].length();
        break;
    }
    case VERSION: {
        ret = sizeof(Version::app) + sizeof(Version::ncs) +
              sizeof(Version::kernel);
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
        Log payload = std::get<Log>(_tx_payload);
        _preprocess_log(payload);
        _tx_payload = payload;
        _new_frame = payload._msg_split.size() > (payload._idx + 1);
        break;
    }
    default: {
        // doesn't need to be split into multiple frames
        _new_frame = false;
        break;
    }
    }
}

void AresFrame::_preprocess_log(Log &payload) {
    if (payload._preprocessed) {
        payload._idx++;
        payload._part++;
        return;
    }

    if (payload.msg.empty()) {
        throw AresFrameError("Log message empty");
    }

    size_t max_msg_size = max_payload_size - Log::_overhead;
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

void AresFrame::_serialize_setting(const Setting &payload,
                                   std::vector<uint8_t> &buffer) {
    SERIALIZE(setting_id);

    if (payload.set) {
        SERIALIZE(value);
    }
}

void AresFrame::_serialize_start(const Start &payload,
                                 std::vector<uint8_t> &buffer) {
    SERIALIZE(sec);
    SERIALIZE(nsec);
    SERIALIZE(id);
    SERIALIZE(broadcast);
    SERIALIZE(seq_cnt);
    SERIALIZE(packet_id);
}

void AresFrame::_serialize_lora_config(const LoraConfig &payload,
                                       std::vector<uint8_t> &buffer) {
    SERIALIZE(frequency);
    SERIALIZE(preamble_length);
    SERIALIZE(bandwidth);
    SERIALIZE(data_rate);
    SERIALIZE(coding_rate);
    SERIALIZE(tx_power);
    SERIALIZE(cad_mode);
    SERIALIZE(cad_num_symbols);
    SERIALIZE(cad_det_peak);
    SERIALIZE(cad_det_min);
}

void AresFrame::_serialize_led(const Led &payload,
                               std::vector<uint8_t> &buffer) {
    SERIALIZE(state);
}

void AresFrame::_serialize_heartbeat(const Heartbeat &payload,
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

void AresFrame::_serialize_claim(const Claim &payload,
                                 std::vector<uint8_t> &buffer) {
    SERIALIZE(id);
}

void AresFrame::_serialize_log(const Log &payload,
                               std::vector<uint8_t> &buffer) {
    SERIALIZE(broadcast);
    SERIALIZE(id);
    SERIALIZE(tx_cnt);
    SERIALIZE(_part);
    SERIALIZE(_num_parts);
    SERIALIZE(log_id);
    buffer.insert(buffer.end(), payload._msg_split[payload._idx].begin(),
                  payload._msg_split[payload._idx].end());
}

void AresFrame::_serialize_version(const Version &payload,
                                   std::vector<uint8_t> &buffer) {
    SERIALIZE(app);
    SERIALIZE(ncs);
    SERIALIZE(kernel);
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
    DESERIALIZE_INIT(Setting);
    DESERIALIZE(setting_id);
    DESERIALIZE(value);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_led(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(Led);
    DESERIALIZE(state);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_start(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(Start);
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
    DESERIALIZE_INIT(Heartbeat, 1);
    DESERIALIZE_SET(ready, (buf[0] & 1) != 0);
    DESERIALIZE_SET(broadcast, (buf[0] & 2) != 0);
    // No need to advance here. Offset is already set to 1 byte...
    DESERIALIZE(tx_cnt);
    DESERIALIZE(id);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_claim(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(Claim);
    DESERIALIZE(id);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_log(const uint8_t *buf, size_t len) {
    DESERIALIZE_INIT(Log);
    DESERIALIZE(broadcast);
    DESERIALIZE(id);
    DESERIALIZE(tx_cnt);
    DESERIALIZE(part);
    DESERIALIZE(num_parts);
    DESERIALIZE(log_id);
    DESERIALIZE_STR(msg, len - Log::_overhead);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_log_ack(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(LogAck);
    DESERIALIZE(part);
    DESERIALIZE(num_parts);
    DESERIALIZE(id);
    DESERIALIZE(log_id);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_version(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(Version);
    DESERIALIZE(app);
    DESERIALIZE(ncs);
    DESERIALIZE(kernel);
    DESERIALIZE_FINALIZE();
}

void AresFrame::_deserialize_ack(const uint8_t *buf, size_t len) {
    AckErrorCode code;
    memcpy(&code, buf, len);
    _rx_payload = code;
}

void AresFrame::_deserialize_framing_error(const uint8_t *buf, size_t len) {
    FramingError error;
    uint32_t mem;
    memcpy(&mem, buf, len);
    error = static_cast<FramingError>(mem);
    _rx_payload = error;
}

void AresFrame::_deserialize_dbg(const uint8_t *buf, size_t len) {
    ARG_UNUSED(len);
    DESERIALIZE_INIT(Dbg);
    DESERIALIZE(code);
    DESERIALIZE_FINALIZE();
}
