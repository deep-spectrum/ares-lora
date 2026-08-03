/**
 * @file frame_types.hpp
 *
 * @brief
 *
 * @date 8/3/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_FRAME_TYPES_HPP
#define ARES_FRAME_TYPES_HPP

#include <exception>
#include <string>

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
} // namespace AresFrame

#endif // ARES_FRAME_TYPES_HPP
