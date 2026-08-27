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
#include <exception>
#include <string>
#include <vector>

namespace AresFrame {

using std::size_t;

/**
 * @class NotSupported
 * Not supported exception.
 */
class NotSupported : public std::exception {
  public:
    /**
     * Constructor.
     * @param msg The error message.
     */
    explicit NotSupported(const char *msg) : _msg(msg) {}

    /**
     * The exception reason.
     * @return The error message.
     */
    [[nodiscard]] const char *what() const noexcept override {
        return _msg.c_str();
    }

  private:
    const std::string _msg;
};

namespace Internal {
/**
 * @struct FramePayloadBase
 * Base class for frame payloads.
 */
struct FramePayloadBase {
    /**
     * Destructor.
     */
    virtual ~FramePayloadBase() = 0;

    /**
     * Default implementation of payload size. This must be overridden if a
     * frame is expected to be transmitted.
     * @return None
     * @throws NotSupported Method not supported by derived class.
     */
    virtual size_t payload_size() {
        throw NotSupported("Payload size is not supported in RX only frames.");
    }

    /**
     * Preprocess a frame payload. This can be used for a variety of
     * applications. Some applications may include, but not limited to:
     *
     * - Splitting up a frame into multiple frames.
     * - Validating parameters
     * .
     *
     * The default behavior is to do nothing.
     */
    virtual void preprocess() {}

    /**
     * Encode into a buffer. This method must be overridden if frames are
     * expected to be transmitted.
     * @param buffer The buffer to place data into.
     * @throws NotSupported Method not supported by derived class.
     */
    virtual void serialize(std::vector<uint8_t> &buffer) {
        throw NotSupported("serialize() is not supported in RX only frames.");
    }

    /**
     * Decode the payload from a serial buffer. This method must be overridden
     * if frames are expected to be received.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     * @throws NotSupported Method not supported by derived class.
     */
    virtual void deserialize(const uint8_t *buffer, std::size_t len) {
        throw NotSupported("deserialize() is not supported in TX only frames.");
    }

    /**
     * Check if there is a new frame available after preprocessing.
     *
     * Default behavior is to return @p false since most messages only need one
     * frame.
     *
     * @return @p true if there is a new frame, @p false otherwise.
     */
    virtual bool new_frame() { return false; }

    /**
     * Get the number of frames needed.
     *
     * Default behavior is to return @p 1 since most messages only need one
     * frame.
     *
     * @return Number of frames.
     */
    [[nodiscard]] virtual size_t num_frames() const { return 1; }
};

inline FramePayloadBase::~FramePayloadBase() = default;
} // namespace Internal
} // namespace AresFrame

#endif // ARES_PAYLOAD_BASE_HPP
