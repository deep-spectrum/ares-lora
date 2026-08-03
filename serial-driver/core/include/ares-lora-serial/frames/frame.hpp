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

#include <ares-lora-serial/frames/ble/ble_chunks.hpp>
#include <ares-lora-serial/frames/ble/ble_connected.hpp>
#include <ares-lora-serial/frames/ble/ble_disconnect.hpp>
#include <ares-lora-serial/frames/ble/ble_image_chunk.hpp>
#include <ares-lora-serial/frames/ble/ble_state.hpp>
#include <ares-lora-serial/frames/ble/ble_subscribed.hpp>
#include <ares-lora-serial/frames/debug/dbg.hpp>
#include <ares-lora-serial/frames/debug/packet_rx.hpp>
#include <ares-lora-serial/frames/debug/packet_tx.hpp>
#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/abort.hpp>
#include <ares-lora-serial/frames/lora/config/node_config.hpp>
#include <ares-lora-serial/frames/lora/config/node_config_poll.hpp>
#include <ares-lora-serial/frames/lora/config/node_config_response.hpp>
#include <ares-lora-serial/frames/lora/log/log.hpp>
#include <ares-lora-serial/frames/lora/log/log_ack.hpp>
#include <ares-lora-serial/frames/lora/lora_ack.hpp>
#include <ares-lora-serial/frames/lora/poll/heartbeat.hpp>
#include <ares-lora-serial/frames/lora/poll/poll.hpp>
#include <ares-lora-serial/frames/lora/ready.hpp>
#include <ares-lora-serial/frames/lora/start.hpp>
#include <ares-lora-serial/frames/mcu/ack.hpp>
#include <ares-lora-serial/frames/mcu/framing_error.hpp>
#include <ares-lora-serial/frames/mcu/led.hpp>
#include <ares-lora-serial/frames/mcu/lora_config.hpp>
#include <ares-lora-serial/frames/mcu/reboot.hpp>
#include <ares-lora-serial/frames/mcu/setting.hpp>
#include <ares-lora-serial/frames/mcu/version.hpp>
#include <cstdint>
#include <functional>
#include <map>
#include <sys/types.h>
#include <tuple>
#include <variant>
#include <vector>

namespace AresFrame {
/**
 * @class Frame
 * Class for encoding and decoding Ares Frames.
 */
class Frame {
  public:
    /**
     * @typedef TxTypes
     *
     * A variant representing all the transmission frame types.
     */
    using TxTypes =
        std::variant<std::monostate, Setting, Start, LoraConfig, Led, Heartbeat,
                     Poll, Log, Version, BleState, BleChunk, BleImage,
                     BleDisconnect, Reboot, Abort, NodeConfig, NodeConfigPoll,
                     NodeConfigResponse, NodeReady, LoraAck>;

    /**
     * @typedef RxTypes
     *
     * A variant representing all the reception frame types.
     */
    using RxTypes =
        std::variant<std::monostate, Setting, Start, Ack, FramingError, Led,
                     Heartbeat, Poll, Log, Version, LogAck, Dbg, PktRx, PktTx,
                     BleState, BleConnect, BleSubscribed, LoraAck, Abort,
                     NodeConfig, NodeConfigPoll, NodeConfigResponse, NodeReady>;

    /**
     * @typedef ResponseTypes
     *
     * A variant representing all the response frame types.
     */
    using ResponseTypes = std::variant<std::monostate, Setting, Ack,
                                       FramingError, Led, Version, BleState>;

    /**
     * Construct a transmit from.
     * @param type The frame type.
     * @param tx_payload The structured payload of the frame.
     */
    explicit Frame(AresFrameType type, const TxTypes &tx_payload);

    /**
     * Constructor for decoding a received frame.
     * @param buffer The buffer to parse the frame from.
     */
    explicit Frame(const std::vector<uint8_t> &buffer);

    /**
     * Constructor.
     */
    Frame() = default;

    /**
     * Copy constructor.
     * @param other Other instance to copy.
     */
    Frame(const Frame &other);

    /**
     * Destructor.
     */
    ~Frame() = default;

    /**
     * Copy assignment operator.
     * @param other The other frame instance to copy.
     * @return This frame.
     */
    Frame &operator=(const Frame &other);

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

    /**
     * Get the receive payload.
     * @return The received frame payload in structured form.
     */
    [[nodiscard]] RxTypes rx_payload() const;

    /**
     * Get the transmit payload.
     * @return The transmit frame payload in structured form.
     */
    [[nodiscard]] TxTypes tx_payload() const;

  private:
    enum FrameDirection { TX, RX, UNSPECIFIED };
    bool _new_frame = true;

    FrameDirection _direction = UNSPECIFIED;
    AresFrameType _type = UNKNOWN;
    TxTypes _tx_payload;
    RxTypes _rx_payload;

    void _preprocess();
    uint16_t _payload_size();

    const std::map<AresFrameType, std::function<RxTypes()>> _rx_map = {
        {SETTING, []() { return Setting(); }},
        {START, []() { return Start(); }},
        {ACK, []() { return Ack(); }},
        {FRAMING_ERROR, []() { return FramingError(); }},
        {LED, []() { return Led(); }},
        {HEARTBEAT, []() { return Heartbeat(); }},
        {POLL, []() { return Poll(); }},
        {LOG, []() { return Log(); }},
        {VERSION, []() { return Version(); }},
        {LOG_ACK, []() { return LogAck(); }},
        {DBG, []() { return Dbg(); }},
        {PKT_RX, []() { return PktRx(); }},
        {PKT_TX, []() { return PktTx(); }},
        {BLE_STATE, []() { return BleState(); }},
        {BLE_CONNECTED, []() { return BleConnect(); }},
        {BLE_SUBSCRIBED, []() { return BleSubscribed(); }},
        {LORA_ACK, []() { return LoraAck(); }},
        {ABORT, []() { return LoraAck(); }},
        {NODE_CONFIG, []() { return NodeConfig(); }},
        {NODE_CONFIG_POLL, []() { return NodeConfigPoll(); }},
        {NODE_CONFIG_RESP, []() { return NodeConfigResponse(); }},
        {NODE_READY, []() { return NodeReady(); }},
    };
};

} // namespace AresFrame

#endif // ARES_FRAME_HPP
