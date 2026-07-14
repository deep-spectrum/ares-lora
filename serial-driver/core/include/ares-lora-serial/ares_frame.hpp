/**
 * @file ares_frame.hpp
 *
 * @brief Ares Frame Library for host computers.
 *
 * @date 3/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_ARES_FRAME_HPP
#define ARES_ARES_FRAME_HPP

#include <cstdint>
#include <exception>
#include <string>
#include <sys/types.h>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

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
 * @class AresFrame
 *
 * Framing class for Ares frames. These frames are used for communication with
 * the Ares LoRa platform.
 */
class AresFrame {
  public:
    /**
     * @enum AresFrameType
     *
     * Frame types for communication with the LoRa module.
     */
    enum AresFrameType : unsigned int {
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

        DRIVER_STOP, ///< Frame used to stop the core driver.
        UNKNOWN,     ///< Unknown frame
    };

    /**
     * @struct Setting
     *
     * Data for AresFrame::SETTING frames.
     */
    struct Setting {
        bool set = false;        ///< Set flag for settings.
        uint16_t setting_id = 0; ///< The setting ID.
        uint32_t value = 0;      ///< The value of the setting.
    };

    /**
     * @struct Start
     *
     * Data for AresFrame::START frames.
     */
    struct Start {
        int64_t sec = -1;       ///< Seconds part for start time.
        uint64_t usec = 0;      ///< Microseconds part for start time.
        uint16_t id = 0;        ///< The destination ID on transmissions. The
                                ///< source ID on reception.
        uint16_t packet_id = 0; ///< Packet ID of the received packet.
                                ///< Ignored on transmissions.
        bool broadcast = false; ///< On transmission, tells firmware to use a
                                ///< broadcast packet. On reception, indicates
                                ///< if received packet was a broadcast.
        uint8_t seq_cnt = 0;    ///< The packet sequence count. Ignored on
                                ///< transmission.
    };

    /**
     * @struct LoraConfig
     *
     * Data for AresFrame::LORA_CONFIG frames.
     */
    struct LoraConfig {
        /**
         * Frequency in Hz to use for transceiving.
         */
        uint32_t frequency = 0;

        /**
         * Length of the preamble.
         */
        uint16_t preamble_length = 0;

        /**
         * The bandwidth to use for transceiving.
         */
        uint8_t bandwidth = 0;

        /**
         * The data-rate to use for transceiving.
         */
        uint8_t data_rate = 0;

        /**
         * The coding rate to use for transceiving.
         */
        uint8_t coding_rate = 0;

        /**
         * TX-power in dBm to use for transmission.
         */
        int8_t tx_power = 0;

        /**
         * Channel Activity Detection mode.
         *
         * Controls whether send/recv operations perform CAD before the actual
         * operation.
         *
         * - `0`: No CAD (default).
         * - `1`: CAD before receive.
         * - `2`: Listen Before Talk. Performs CAD before transmitting.
         *
         * @note Not implemented in firmware.
         */
        uint8_t cad_mode = 0;

        /**
         * Number of symbols for CAD detection.
         *
         * @note Not implemented in firmware.
         */
        uint8_t cad_num_symbols = 0;

        /**
         * Detection peak threshold (hardware-specific, dimensionless).
         *
         * @note Not implemented in firmware.
         */
        uint8_t cad_det_peak = 0;

        /**
         * Minimum detection threshold (hardware-specific, dimensionless).
         *
         * @note Not implemented in firmware.
         */
        uint8_t cad_det_min = 0;
    };

    /**
     * @struct Led
     *
     * Data for AresFrame::LED frames.
     */
    struct Led {
        /**
         * @enum LedState
         *
         * LED states.
         */
        enum LedState : uint8_t {
            OFF = 0,   ///< LED off.
            ON = 1,    ///< LED on.
            BLINK = 2, ///< LED blinking at 1 Hz.
            FADE = 3,  ///< LED fading.
            FETCH = 4, ///< Retrieve LED state from firmware.
        };

        /**
         * The LED number/ID.
         */
        uint8_t led = 0;

        /**
         * The LED state frame data.
         */
        LedState state = FETCH;
    };

    /**
     * @struct Heartbeat
     *
     * Data for AresFrame::HEARTBEAT frames.
     */
    struct Heartbeat {
        /**
         * System ready for data collection.
         */
        bool ready = false;

        /**
         * The number of times to send the heartbeat.
         */
        uint8_t tx_cnt = 0;

        /**
         * The destination for the heartbeat.
         */
        uint16_t id = 0;
    };

    /**
     * @struct Poll
     *
     * Data for AresFrame::POLL frames.
     */
    struct Poll {
        /**
         * When transmitting, the ID of the node to poll for a heartbeat. When
         * receiving, the source of the poll message.
         */
        uint16_t id = 0;
    };

    /**
     * @struct Log
     *
     * Data for AresFrame::LOG frames.
     */
    struct Log {
        /**
         * Constructor.
         * @param broadcast Flag indicating if the message should be
         * broadcasted.
         * @param tx_cnt The number of times to send the message over LoRa.
         * @param id The destination or source ID.
         * @param log_id The ID of the log message.
         * @param msg The log message.
         */
        Log(bool broadcast, uint8_t tx_cnt, uint16_t id, uint16_t log_id,
            std::string msg)
            : broadcast(broadcast), tx_cnt(tx_cnt), id(id), log_id(log_id),
              msg(std::move(msg)) {}

        /**
         * Default constructor.
         */
        Log() = default;

        /**
         * Flag indicating if the message was/should be broadcasted.
         */
        bool broadcast = false;

        /**
         * The number of times to transmit the message.
         */
        uint8_t tx_cnt = 1;

        /**
         * The chunk ID of the message. 1 indexed and automatically managed on
         * serialization.
         */
        uint8_t part = 1;

        /**
         * The number of chunks in the log message. Automatically managed on
         * serialization.
         */
        uint8_t num_parts = 1;

        /**
         * On transmission, the ID of the node to send the message to if not
         * broadcasting. On reception, the ID of the node the message was sent
         * from.
         */
        uint16_t id = 0;

        /**
         * The ID of the log message.
         */
        uint16_t log_id = 0;

        /**
         * The entire log message on transmission. The log message chunk on
         * reception.
         */
        std::string msg;

        friend class AresFrame;

      private:
        std::vector<std::string> _msg_split;
        size_t _idx = 0;
        // used for serialization
        uint8_t _part = 0;
        uint8_t _num_parts = 1;
        bool _preprocessed = false;
        static constexpr size_t _overhead = sizeof(broadcast) + sizeof(part) +
                                            sizeof(num_parts) + sizeof(id) +
                                            sizeof(tx_cnt) + sizeof(log_id);
    };

    /**
     * @struct LogAck
     *
     * Data for AresFrame::LOG_ACK frames.
     */
    struct LogAck {
        /**
         * The message part that was acknowledged.
         */
        uint8_t part = 0;

        /**
         * The total number of parts in the acked message.
         */
        uint8_t num_parts = 0;

        /**
         * The ID of the node that acknowledged the message.
         */
        uint16_t id = 0;

        /**
         * The ID of the log message that got acked.
         */
        uint16_t log_id = 0;

        /**
         * Equivalence operator.
         * @param other The other object to compare against.
         * @return `true` if all the fields are equal, `false` otherwise.
         */
        bool operator==(const LogAck &other) const {
            return (part == other.part) && (num_parts == other.num_parts) &&
                   (id == other.id) && (log_id == other.log_id);
        }
    };

    /**
     * @struct Version
     *
     * Data for AresFrame::VERSION frames.
     */
    struct Version {
        /**
         * The application version.
         */
        uint32_t app = 0;

        /**
         * The Nordic Connect SDK version.
         */
        uint32_t ncs = 0;

        /**
         * The Zephyr RTOS kernel version.
         */
        uint32_t kernel = 0;
    };

    /**
     * @typedef AckErrorCode
     *
     * Data for AresFrame::ACK frames.
     */
    using AckErrorCode = int32_t;

    /**
     * @enum FramingError
     *
     * Data for AresFrame::FRAMING_ERROR frames.
     */
    enum FramingError : uint8_t {
        BAD_FRAME = 0,       ///< Bad frame.
        BAD_TYPE = 1,        ///< Bad frame type.
        NOT_IMPLEMENTED = 2, ///< Frame type not implemented.
    };

    /**
     * @struct Dbg
     *
     * Data for AresFrame::DBG frames.
     */
    struct Dbg {
        /**
         * Error code from firmware.
         */
        int32_t code = 0;
    };

    /**
     * @struct PktRx
     *
     * Data for AresFrame::PKT_RX frames.
     */
    struct PktRx {
        /**
         * Packet ID.
         */
        uint16_t packet_id = 0;
        /**
         * Packet source ID.
         */
        uint16_t src_id = 0;

        /**
         * Sequence counter.
         */
        uint8_t seq_cnt = 0;
    };

    /**
     * @struct PktRx
     *
     * Data for AresFrame::PKT_TX frames.
     */
    struct PktTx {
        /**
         * Transmit count.
         */
        uint32_t count = 0;
    };

    /**
     * @struct BleState
     *
     * Payload data for AresFrame::BLE_STATE frames.
     */
    struct BleState {
        /**
         * @enum State
         *
         * BLE States.
         */
        enum State : uint8_t {
            OFF = 0,     ///< BLE off.
            ON = 1,      ///< BLE On.
            REQUEST = 2, ///< Request BLE state.
        };

        /**
         * Default constructor.
         */
        BleState() = default;

        /**
         * Constructor.
         * @param[in] value The state value.
         */
        explicit BleState(uint8_t value) { state = static_cast<State>(value); }

        /**
         * The BLE state frame data.
         */
        State state = REQUEST;
    };

    /**
     * @struct BleConnect
     *
     * Payload data for AresFrame::BLE_CONNECTED frames.
     */
    struct BleConnect {
        /**
         * Flag indicating if the BLE is connected or not.
         */
        bool connected = false;

        /**
         * The maximum transfer size.
         */
        uint16_t chunk_size = 0;
    };

    /**
     * @struct BleSubscribed
     *
     * Payload data for AresFrame::BLE_SUBSCRIBED frames.
     */
    struct BleSubscribed {
        /**
         * Flag indicating if the chunks attribute has been subscribed to.
         */
        bool chunk = false;

        /**
         * Flag indicating if the image attribute has been subscribed to.
         */
        bool image = false;
    };

    /**
     * @struct BleChunk
     *
     * Payload data for AresFrame::BLE_CHUNK frames.
     */
    struct BleChunk {
        /**
         * The number of chunks to transfer.
         */
        uint64_t num_chunks = 0;
    };

    /**
     * @struct BleImage
     *
     * Payload data for AresFrame::BLE_IMAGE_CHUNK frames.
     */
    struct BleImage {
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

        friend class AresFrame;

      private:
        std::vector<std::vector<uint8_t>> _img_split;
        size_t _idx = 0;
        // used for serialization
        uint64_t _num_chunks = 1;
        bool _preprocessed = false;
        uint16_t _max_chunk_size = 29;
    };

    /**
     * @typedef TxTypes
     *
     * A variant representing all the transmission frame types.
     */
    using TxTypes =
        std::variant<std::monostate, Setting, Start, LoraConfig, Led, Heartbeat,
                     Poll, Log, Version, BleState, BleChunk, BleImage>;

    /**
     * @typedef RxTypes
     *
     * A variant representing all the reception frame types.
     */
    using RxTypes =
        std::variant<std::monostate, Setting, Start, AckErrorCode, FramingError,
                     Led, Heartbeat, Poll, Log, Version, LogAck, Dbg, PktRx,
                     PktTx, BleState, BleConnect, BleSubscribed>;

    /**
     * @typedef ResponseTypes
     *
     * A variant representing all the response frame types.
     */
    using ResponseTypes = std::variant<std::monostate, Setting, AckErrorCode,
                                       FramingError, Led, Version, BleState>;

    /**
     * @struct Decoded
     *
     * Struct containing the deserialized frame data.
     */
    struct Decoded {
        /**
         * The frame type.
         */
        AresFrameType type;

        /**
         * The payload data. This is linked to the frame type.
         */
        RxTypes payload;
    };

    /**
     * Constructs a frame object from deserialized data.
     * @param type The type of the frame.
     * @param payload The payload for the frame.
     */
    explicit AresFrame(AresFrameType type, TxTypes payload);

    /**
     * Constructs a frame object from serial data.
     * @param bytearray The serial data.
     *
     * @throws AresFrameError if frame is not found in buffer.
     */
    explicit AresFrame(const std::vector<uint8_t> &bytearray);

    /**
     * Default constructor.
     */
    AresFrame();

    /**
     * Copy constructor.
     * @param other Other object to copy.
     */
    AresFrame(const AresFrame &other);

    /**
     * Destructor.
     */
    ~AresFrame() = default;

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

  private:
    enum FrameDirection { TX, RX, UNSPECIFIED };
    bool _new_frame = true;

    FrameDirection _direction = UNSPECIFIED;
    AresFrameType _type = UNKNOWN;
    TxTypes _tx_payload;
    RxTypes _rx_payload;

    [[nodiscard]] uint16_t _payload_size() const;

    void _preprocess_serialize();
    static void _preprocess_log(Log &payload);
    static void _preprocess_ble_image(BleImage &payload);

    static void _serialize_setting(const Setting &payload,
                                   std::vector<uint8_t> &buffer);
    static void _serialize_start(const Start &payload,
                                 std::vector<uint8_t> &buffer);
    static void _serialize_lora_config(const LoraConfig &payload,
                                       std::vector<uint8_t> &buffer);
    static void _serialize_led(const Led &payload,
                               std::vector<uint8_t> &buffer);
    static void _serialize_heartbeat(const Heartbeat &payload,
                                     std::vector<uint8_t> &buffer);
    static void _serialize_poll(const Poll &payload,
                                std::vector<uint8_t> &buffer);
    static void _serialize_log(const Log &payload,
                               std::vector<uint8_t> &buffer);
    static void _serialize_version(const Version &payload,
                                   std::vector<uint8_t> &buffer);
    static void _serialize_ble_state(const BleState &payload,
                                     std::vector<uint8_t> &buffer);
    static void _serialize_ble_chunks(const BleChunk &payload,
                                      std::vector<uint8_t> &buffer);
    static void _serialize_ble_image(const BleImage &payload,
                                     std::vector<uint8_t> &buffer);

    void _deserialize_setting(const uint8_t *buf, size_t len);
    void _deserialize_led(const uint8_t *buf, size_t len);
    void _deserialize_start(const uint8_t *buf, size_t len);
    void _deserialize_heartbeat(const uint8_t *buf, size_t len);
    void _deserialize_poll(const uint8_t *buf, size_t len);
    void _deserialize_log(const uint8_t *buf, size_t len);
    void _deserialize_log_ack(const uint8_t *buf, size_t len);
    void _deserialize_version(const uint8_t *buf, size_t len);
    void _deserialize_ack(const uint8_t *buf, size_t len);
    void _deserialize_framing_error(const uint8_t *buf, size_t len);
    void _deserialize_dbg(const uint8_t *buf, size_t len);
    void _deserialize_pkt_rx(const uint8_t *buf, size_t len);
    void _deserialize_pkt_tx(const uint8_t *buf, size_t len);
    void _deserialize_ble_state(const uint8_t *buf, size_t len);
    void _deserialize_ble_connected(const uint8_t *buf, size_t len);
    void _deserialize_ble_subscribed(const uint8_t *buf, size_t len);
};

#endif // ARES_ARES_FRAME_HPP
