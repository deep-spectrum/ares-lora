/**
 * @file ble_image_chunk.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/ble/ble_image_chunk.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
size_t BleImage::num_chunks(const std::vector<uint8_t> &image,
                            uint16_t max_chunk_size) {
    return (image.size() + (static_cast<size_t>(max_chunk_size) - 1)) /
           static_cast<size_t>(max_chunk_size);
}

size_t BleImage::payload_size() { return _img_split[_idx].size(); }

void BleImage::serialize(std::vector<uint8_t> &buffer) {
    buffer.insert(buffer.end(), _img_split[_idx].begin(),
                  _img_split[_idx].end());
}

void BleImage::preprocess() {
    if (_preprocessed) {
        _idx++;
        return;
    }

    if (image.empty()) {
        // todo
    }

    size_t chunks = num_chunks(image, _max_chunk_size);

    _img_split.reserve(chunks);

    ssize_t start = 0;
    for (ssize_t i = 0; i < (chunks - 1); i++, start += _max_chunk_size) {
        _img_split.emplace_back(image.begin() + start,
                                image.begin() + start + _max_chunk_size);
    }

    _img_split.emplace_back(image.begin() + start, image.end());

    _preprocessed = true;
}

bool BleImage::new_frame() { return _img_split.size() > (_idx + 1); }
} // namespace AresFrame
