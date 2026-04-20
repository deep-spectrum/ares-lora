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
        SETTING = 0,        ///< Setting get/set
        START = 1,          ///< Start time
        LORA_CONFIG = 2,    ///< LoRa modem configuration
        LED = 3,            ///< LED state get/set
        HEARTBEAT = 4,      ///< Send heartbeat
        CLAIM = 5,          ///< Master claim
        LOG = 6,            ///< Log message
        LOG_ACK = 7,        ///< Log acknowledge
        VERSION = 8,        ///< Firmware version
        ACK = 9,            ///< Command acknowledge
        FRAMING_ERROR = 10, ///< Framing error
        DBG = 11,           ///< Debug message
        UNKNOWN,            /// Unknown frame
    };

    /**
     * @struct Setting
     */
    struct Setting {
        Setting() = default;

        bool set = false;
        uint16_t setting_id = 0;
        uint32_t value = 0;
    };

    struct Start {
        Start() = default;

        int64_t sec = -1;
        uint64_t nsec = 0;
        uint16_t id = 0;
        uint16_t packet_id = 0;
        bool broadcast = false;
        uint8_t seq_cnt = 0;
    };

    struct LoraConfig {
        LoraConfig() = default;

        uint32_t frequency = 0;
        uint16_t preamble_length = 0;
        uint8_t bandwidth = 0;
        uint8_t data_rate = 0;
        uint8_t coding_rate = 0;
        int8_t tx_power = 0;
        uint8_t cad_mode = 0;
        uint8_t cad_num_symbols = 0;
        uint8_t cad_det_peak = 0;
        uint8_t cad_det_min = 0;
    };

    struct Led {
        enum LedState : uint8_t {
            OFF = 0,
            ON = 1,
            BLINK = 2,
            FETCH = 3,
        };

        LedState state = FETCH;
    };

    struct Heartbeat {
        Heartbeat() = default;

        bool ready = false;
        bool broadcast = false;
        uint8_t tx_cnt = 0;
        uint16_t id = 0;
    };

    struct Claim {
        uint16_t id = 0;
    };

    struct Log {
        Log(bool broadcast, uint8_t tx_cnt, uint16_t id, uint16_t log_id,
            std::string msg)
            : broadcast(broadcast), tx_cnt(tx_cnt), id(id), log_id(log_id),
              msg(std::move(msg)) {}
        Log() = default;

        bool broadcast = false;
        uint8_t tx_cnt = 1;
        uint8_t part = 1;      // 1 indexed. Automatically managed.
        uint8_t num_parts = 1; // starts at 1. Automatically managed.
        uint16_t id = 0;
        uint16_t log_id = 0;
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

    struct LogAck {
        uint8_t part = 0;
        uint8_t num_parts = 0;
        uint16_t id = 0;
        uint16_t log_id = 0;

        bool operator==(const LogAck &other) const {
            return (part == other.part) && (num_parts == other.num_parts) &&
                   (id == other.id) && (log_id == other.log_id);
        }
    };

    struct Version {
        uint32_t app = 0;
        uint32_t ncs = 0;
        uint32_t kernel = 0;
    };

    using AckErrorCode = int32_t;

    enum FramingError : uint8_t {
        BAD_FRAME = 0,
        BAD_TYPE = 1,
        NOT_IMPLEMENTED = 2,
    };

    struct Dbg {
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
