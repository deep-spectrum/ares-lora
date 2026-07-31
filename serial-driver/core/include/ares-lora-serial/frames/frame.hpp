/**
 * @file frame.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_FRAME_HPP
#define ARES_FRAME_HPP

#include <cstdint>
#include <exception>
#include <string>
#include <sys/types.h>
#include <tuple>
#include <utility>
#include <vector>

namespace AresFrame {
/**
 * @class AresFrameError
 *
 * Exception class for AresFrame.
 */
class AresFrameError : public std::exception {
  public:
    /**
     * Constructor.
     * @param msg The error message.
     */
    explicit AresFrameError(std::string msg) : msg_(std::move(msg)) {}

    /**
     * Retrieve the error message.
     * @return The error message.
     */
    [[nodiscard]] const char *what() const noexcept override {
        return msg_.c_str();
    }

  private:
    std::string msg_;
};

/**
 * @enum AresFrameType
 *
 * Frame types for communication with the LoRa module.
 */
enum AresFrameType : uint16_t {
    SETTING = 0,        ///< Setting get/set (TX/RX)
    START = 1,          ///< Start time (TX/RX)
    LORA_CONFIG = 2,    ///< LoRa modem configuration (TX)
    LED = 3,            ///< LED state get/set (TX/RX)
    HEARTBEAT = 4,      ///< Send heartbeat (TX/RX)
    POLL = 5,           ///< Poll a node for a heartbeat (TX/RX)
    LOG = 6,            ///< Log message (TX/RX)
    LOG_ACK = 7,        ///< Log acknowledge (RX)
    VERSION = 8,        ///< Firmware version (TX/RX)
    ACK = 9,            ///< Command acknowledge (RX)
    FRAMING_ERROR = 10, ///< Framing error (RX)
    DBG = 11,           ///< Debug message (RX)
    PKT_RX = 12,        ///< Packet Received (RX)
    PKT_TX = 13,        ///< Packet transmitted (RX)

    BLE_STATE = 14,       ///< Set or retrieve the BLE state (TX/RX)
    BLE_CONNECTED = 15,   ///< BLE connect state change (RX)
    BLE_DISCONNECT = 16,  ///< Disconnect BLE (TX)
    BLE_SUBSCRIBED = 17,  ///< BLE service subscription change (RX)
    BLE_CHUNK = 18,       ///< BLE tell central how many chunks (TX)
    BLE_IMAGE_CHUNK = 19, ///< BLE transfer image chunk (TX)

    REBOOT = 20,           ///< Reboot (TX)
    LORA_ACK = 21,         ///< LoRa packet Acknowledge (TX/RX)
    ABORT = 22,            ///< Abort measurement (TX/RX)
    NODE_CONFIG = 23,      ///< Configure receiver node (TX/RX)
    NODE_CONFIG_POLL = 24, ///< Poll a receiver node's configs (TX/RX)
    NODE_CONFIG_RESP = 25, ///< Poll response for configurations
    ///< poll (TX/RX)
    NODE_READY = 26, ///< Indication that the coordinator is ready to start
    ///< reception (TX/RX)

    DRIVER_STOP, ///< Frame used to stop the core driver.
    UNKNOWN,     ///< Unknown frame
};

class Frame {
  public:
    struct Decoded {
        AresFrameType type;
        // todo: figure something out here. variant is fucking annoying
    };

    explicit Frame(AresFrameType type /* todo */);
    explicit Frame(const std::vector<uint8_t> &buffer);
    Frame();
    Frame(const Frame &other);
    ~Frame() = default;

    /**
     * Checks if there is a frame present in the given buffer.
     * @param serial_data The serial data buffer to check.
     * @param len The length of the serial data buffer.
     * @param error_no_footer Return an error if there is no footer.
     * @return std::tuple<header index, frame size, bytes left> if frame found.
     * @return std::tuple<-1, -1, -1> on no frame found.
     */
    static std::tuple<ssize_t, ssize_t, ssize_t>
    frame_present(const uint8_t *serial_data, size_t len,
                  bool error_no_footer = true);

    /**
     * Checks if there is a frame present in the given buffer.
     * @param bytearray The buffer to check.
     * @param error_no_footer Return an error if there is no footer.
     * @return std::tuple<header index, frame size, bytes left> if frame found.
     * @return std::tuple<-1, -1, -1> on no frame found.
     */
    static std::tuple<ssize_t, ssize_t, ssize_t>
    frame_present(const std::vector<uint8_t> &bytearray,
                  bool error_no_footer = true);

    /**
     * Serialize the frame into a buffer. If a frame is split into chunks, then
     * places the next frame into the buffer.
     * @param bytearray The buffer to store the serialized frame in.
     *
     * @throws AresFrameError if frame type cannot be serialized (meant for
     * reception only).
     * @throws AresFrameError if log message is empty.
     * @throws AresFrameError if log message is too long.
     * @throws AresFrameError if frame payload length cannot be calculated.
     */
    void serialize(std::vector<uint8_t> &bytearray);

    /**
     * Parse a frame from the given buffer.
     * @param serial_data The buffer to parse a frame from.
     * @param start_index The start index of the frame.
     * @param len The length of the buffer.
     *
     * @throws AresFrameError if frame type cannot be parsed (meant for
     * transmission only).
     */
    void parse(const uint8_t *serial_data, size_t start_index, size_t len);

    /**
     * Parse a frame from the given buffer.
     * @param bytearray The buffer to parse a frame from
     * @param start_index The start index of the frame.
     *
     * @throws AresFrameError if frame type cannot be parsed (meant for
     * transmission only).
     */
    void parse(const std::vector<uint8_t> &bytearray, size_t start_index);

    /**
     * Retrieve the parsed frame.
     * @return The decoded or parsed frame.
     *
     * @note AresFrame::parse must be called first.
     */
    [[nodiscard]] Decoded get_parsed_frame() const;

    /**
     * Check if a new frame is available for serialization. Useful for messages
     * split into multiple frames.
     * @return `true` if a new frame is available for serialization. `false`
     * otherwise.
     */
    [[nodiscard]] bool frame_available() const;

    /**
     * Retrieve the number of frames a message is split up into.
     * @return The number of frames that can be serialized and sent.
     *
     * @note AresFrame::serialize must be called first.
     */
    [[nodiscard]] size_t total_frames() const;

    /**
     * Retrieve the frame type without decoding.
     * @return The frame type.
     */
    [[nodiscard]] AresFrameType type() const;

    // todo: get payload???

  private:
    enum FrameDirection { TX, RX, UNSPECIFIED };
    bool _new_frame = true;

    FrameDirection _direction = UNSPECIFIED;
    AresFrameType _type = UNKNOWN;

    // todo: How do I store the payload???
};

} // namespace AresFrame

#endif // ARES_FRAME_HPP
