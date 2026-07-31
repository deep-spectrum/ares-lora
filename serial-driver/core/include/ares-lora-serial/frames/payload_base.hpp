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

class NotSupported : std::exception {
  public:
    explicit NotSupported(const char *msg) : _msg(msg) {}

    [[nodiscard]] const char *what() const noexcept override {
        return _msg.c_str();
    }

  private:
    const std::string _msg;
};

namespace Internal {
struct FramePayloadBase {
    virtual ~FramePayloadBase() = 0;

    virtual size_t payload_size() {
        throw NotSupported("Payload size is not supported in RX only frames.");
    }

    virtual void preprocess() {
        // default behavior is no preprocessing necessary.
        // recommended uses:
        // - validation of parameters
        // - Splitting up a message to be sent over multiple frames.
    }

    virtual void serialize(std::vector<uint8_t> &buffer) {
        throw NotSupported("serialize() is not supported in RX only frames.");
    }

    virtual void deserialize(const uint8_t *buffer, std::size_t len) {
        throw NotSupported("deserialize() is not supported in TX only frames.");
    }

    virtual bool new_frame() {
        // default behavior is single frame is needed.
        return false;
    }
};

inline FramePayloadBase::~FramePayloadBase() = default;
} // namespace Internal
} // namespace AresFrame

#endif // ARES_PAYLOAD_BASE_HPP
