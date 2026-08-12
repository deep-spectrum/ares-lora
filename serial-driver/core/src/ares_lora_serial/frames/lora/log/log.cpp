/**
 * @file log.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/log/log.hpp>
#include <ares/serialization.hpp>
#include <cassert>

namespace AresFrame {
size_t Log::payload_size() { return _overhead + _msg_split[_idx].length(); }

void Log::preprocess() {
    if (_preprocessed) {
        _idx++;
        _part++;
        return;
    }

    if (msg.empty()) {
        throw AresFrameError("Empty log message");
    }

    size_t max_msg_size = _max_payload_size - _overhead;
    size_t num_substr = (msg.size() + (max_msg_size - 1)) / max_msg_size;

    if (num_substr > static_cast<size_t>(UINT8_MAX)) {
        throw AresFrameError("Log message too large");
    }

    _msg_split.reserve(num_substr);
    std::string_view content = msg;
    size_t i = 0;

    do {
        _msg_split.emplace_back(content.substr(i, max_msg_size));
        i += _msg_split.back().size();
    } while (i < msg.size());

    _part = 1;
    _idx = 0;
    _num_parts = _msg_split.size();
    _preprocessed = true;
}

void Log::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, broadcast, id, tx_cnt, _part, _num_parts, log_id);
    buffer.insert(buffer.end(), _msg_split[_idx].begin(),
                  _msg_split[_idx].end());
    assert(buffer.size() == payload_size());
}

void Log::deserialize(const uint8_t *buffer, std::size_t len) {
    ares::deserialize(buffer, broadcast, id, tx_cnt, part, num_parts, log_id);
    msg = std::string(buffer + _overhead, buffer + len);
}

bool Log::new_frame() { return _msg_split.size() > (_idx + 1); }

size_t Log::num_frames() const { return _num_parts; }

Log::response_type Log::expected_response() const {
    return response_type{static_cast<uint8_t>(_part - 1), _num_parts, id};
}
} // namespace AresFrame
