/**
 * @file payload_base.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_PAYLOAD_BASE_HPP
#define ARES_PAYLOAD_BASE_HPP

#include <cstdint>
#include <vector>

namespace AresFrame::Internal {
struct FramePayloadBase {
    virtual ~FramePayloadBase() = default;

    virtual std::size_t payload_size() = 0;
    virtual void preprocess() = 0;
    virtual void serialize(std::vector<uint8_t> &buffer) = 0;
    virtual void deserialize(const uint8_t *buffer, std::size_t len) = 0;
    virtual bool new_frame() = 0;
};
} // namespace AresFrame::Internal

#endif // ARES_PAYLOAD_BASE_HPP
