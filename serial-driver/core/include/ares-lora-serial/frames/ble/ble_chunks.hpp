/**
 * @file ble_chunks.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_CHUNKS_HPP
#define ARES_BLE_CHUNKS_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct BleChunk : Internal::FramePayloadBase {
    explicit BleChunk(uint64_t num_chunks_) : num_chunks(num_chunks_) {}

    /**
     * The number of chunks to transfer.
     */
    uint64_t num_chunks = 0;

    size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;

  private:
    static constexpr size_t _payload_size = 8;
};
} // namespace AresFrame

#endif // ARES_BLE_CHUNKS_HPP
