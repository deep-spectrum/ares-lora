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

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct FramingError
 * Data for AresFrame::FRAMING_ERROR frames.
 */
struct FramingError : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = FRAMING_ERROR;

    /**
     * @enum ErrorType
     * The different framing errors.
     */
    enum ErrorType : uint32_t {
        /**
         * Bad frame.
         */
        BAD_FRAME = 0,
        /**
         * Bad frame type.
         */
        BAD_TYPE = 1,

        /**
         * Frame type not implemented.
         */
        NOT_IMPLEMENTED = 2,
    };

    /**
     * The framing error that occurred.
     */
    ErrorType type = NOT_IMPLEMENTED;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 4;
};
} // namespace AresFrame

#endif // ARES_FRAMING_ERROR_HPP
