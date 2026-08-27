/**
 * @file ble_image_chunk.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_IMAGE_CHUNK_HPP
#define ARES_BLE_IMAGE_CHUNK_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct BleImage
 *
 * Payload data for AresFrame::BLE_IMAGE_CHUNK frames.
 */
struct BleImage : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = BLE_IMAGE_CHUNK;

    /**
     * Constructor.
     * @param[in] bytes The bytes in the image.
     * @param[in] max_chunk_size The maximum chunk size for a single
     * transfer. This is usually the MTU or less.
     */
    BleImage(const std::vector<uint8_t> &bytes, uint16_t max_chunk_size)
        : image(bytes), _max_chunk_size(max_chunk_size) {}

    /**
     * Constructor.
     */
    BleImage() = default;

    /**
     * The image bytes.
     */
    std::vector<uint8_t> image;

    /**
     * Helper for calculating the number of chunks needed to transfer an
     * image.
     * @param[in] image The image representation in memory.
     * @param[in] max_chunk_size The maximum chunk size.
     * @return The number of chunks needed to transfer the entire image.
     */
    static size_t num_chunks(const std::vector<uint8_t> &image,
                             uint16_t max_chunk_size);

    /**
     * Payload size.
     * @return The payload size.
     */
    size_t payload_size() override;

    /**
     * Encode into a buffer.
     * @param buffer The buffer to place data into.
     */
    void serialize(std::vector<uint8_t> &buffer) override;

    /**
     * Preprocess The payload.
     */
    void preprocess() override;

    /**
     * Check if there is a new frame available after preprocessing.
     * @return @p true if there is a new frame, @p false otherwise.
     */
    bool new_frame() override;

    /**
     * Get the number of frames needed.
     * @return Number of frames.
     */
    [[nodiscard]] size_t num_frames() const override;

  private:
    std::vector<std::vector<uint8_t>> _img_split;
    size_t _idx = 0;
    // used for serialization
    uint64_t _num_chunks = 1;
    bool _preprocessed = false;
    uint16_t _max_chunk_size = 29;
};
} // namespace AresFrame

#endif // ARES_BLE_IMAGE_CHUNK_HPP
