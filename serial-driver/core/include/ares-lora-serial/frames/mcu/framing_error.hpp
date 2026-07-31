/**
 * @file framing_error.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_FRAMING_ERROR_HPP
#define ARES_FRAMING_ERROR_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct FramingError : Internal::FramePayloadBase {
    enum ErrorType : uint32_t {
        BAD_FRAME = 0,
        BAD_TYPE = 1,
        NOT_IMPLEMENTED = 2,
    };

    ErrorType type = NOT_IMPLEMENTED;

    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 4;
};
} // namespace AresFrame

#endif // ARES_FRAMING_ERROR_HPP
