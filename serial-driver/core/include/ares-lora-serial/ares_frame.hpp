/**
 * @file ares_frame.hpp
 *
 * @brief
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

class AresFrameError : public std::exception {
  public:
    explicit AresFrameError(std::string msg) : msg_(std::move(msg)) {}

    [[nodiscard]] const char *what() const noexcept override {
        return msg_.c_str();
    }

  private:
    std::string msg_;
};

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
        CLAIM = 5,          ///< Master claim (TX/RX)
        LOG = 6,            ///< Log message (TX/RX)
        LOG_ACK = 7,        ///< Log acknowledge (RX)
        VERSION = 8,        ///< Firmware version (TX/RX)
        ACK = 9,            ///< Command acknowledge (RX)
        FRAMING_ERROR = 10, ///< Framing error (RX)
        DBG = 11,           ///< Debug message (RX)
        UNKNOWN,            ///< Unknown frame
    };

    /**
     * @struct Setting
     *
     * Data for AresFrame::SETTING frames.
     */
    struct Setting {
        Setting() = default;

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
        Start() = default;

        int64_t sec = -1;       ///< Seconds part for start time.
        uint64_t nsec = 0;      ///< Nanoseconds part for start time.
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
        LoraConfig() = default;

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
            FETCH = 3, ///< Retrieve LED state from firmware.
        };

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
        Heartbeat() = default;

        /**
         * System ready for data collection.
         */
        bool ready = false;

        /**
         * Broadcast the heartbeat frame.
         */
        bool broadcast = false;

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
     * @struct Claim
     *
     * Data for AresFrame::CLAIM frames.
     */
    struct Claim {
        /**
         * When transmitting, the ID of the node to send the claim notification
         * to. When receiving, the claimed master node ID.
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
         * .
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
     * @struct AckErrorCode
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
        int32_t code;
    };

    using TxTypes = std::variant<std::monostate, Setting, Start, LoraConfig,
                                 Led, Heartbeat, Claim, Log, Version>;
    using RxTypes =
        std::variant<std::monostate, Setting, Start, AckErrorCode, FramingError,
                     Led, Heartbeat, Claim, Log, Version, LogAck, Dbg>;

    using ResponseTypes = std::variant<std::monostate, Setting, AckErrorCode,
                                       FramingError, Led, Version>;

    struct Decoded {
        AresFrameType type;
        RxTypes payload;
    };

    explicit AresFrame(AresFrameType type, TxTypes payload);
    explicit AresFrame(const std::vector<uint8_t> &bytearray);
    AresFrame();
    AresFrame(const AresFrame &other);
    ~AresFrame() = default;

    static std::tuple<ssize_t, ssize_t, ssize_t>
    frame_present(const uint8_t *serial_data, size_t len,
                  bool error_no_footer = true);
    static std::tuple<ssize_t, ssize_t, ssize_t>
    frame_present(const std::vector<uint8_t> &bytearray,
                  bool error_no_footer = true);

    void serialize(std::vector<uint8_t> &bytearray);
    void parse(const uint8_t *serial_data, size_t start_index, size_t len);
    void parse(const std::vector<uint8_t> &bytearray, size_t start_index);

    [[nodiscard]] Decoded get_parsed_frame() const;

    [[nodiscard]] bool frame_available() const;

    [[nodiscard]] size_t total_frames() const;

  private:
    enum FrameDirection { TX, RX, UNSPECIFIED };
    bool _new_frame = true;

    FrameDirection _direction;
    AresFrameType _type;
    TxTypes _tx_payload;
    RxTypes _rx_payload;

    [[nodiscard]] uint16_t _payload_size() const;

    void _preprocess_serialize();
    static void _preprocess_log(Log &payload);

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
    static void _serialize_claim(const Claim &payload,
                                 std::vector<uint8_t> &buffer);
    static void _serialize_log(const Log &payload,
                               std::vector<uint8_t> &buffer);
    static void _serialize_version(const Version &payload,
                                   std::vector<uint8_t> &buffer);

    void _deserialize_setting(const uint8_t *buf, size_t len);
    void _deserialize_led(const uint8_t *buf, size_t len);
    void _deserialize_start(const uint8_t *buf, size_t len);
    void _deserialize_heartbeat(const uint8_t *buf, size_t len);
    void _deserialize_claim(const uint8_t *buf, size_t len);
    void _deserialize_log(const uint8_t *buf, size_t len);
    void _deserialize_log_ack(const uint8_t *buf, size_t len);
    void _deserialize_version(const uint8_t *buf, size_t len);
    void _deserialize_ack(const uint8_t *buf, size_t len);
    void _deserialize_framing_error(const uint8_t *buf, size_t len);
    void _deserialize_dbg(const uint8_t *buf, size_t len);
};

#endif // ARES_ARES_FRAME_HPP
