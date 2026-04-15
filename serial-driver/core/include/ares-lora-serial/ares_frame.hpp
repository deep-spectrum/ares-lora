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
    enum AresFrameType : unsigned int {
        SETTING = 0,
        START = 1,
        LORA_CONFIG = 2,
        LED = 3,
        HEARTBEAT = 4,
        CLAIM = 5,
        LOG = 6,
        ACK = 7,
        FRAMING_ERROR = 8,
        UNKNOWN,
    };

    struct AresFrameSetting {
        AresFrameSetting() = default;

        bool set = false;
        uint16_t setting_id = 0;
        uint32_t value = 0;
    };

    struct AresFrameStart {
        AresFrameStart() = default;

        int64_t sec = -1;
        uint64_t nsec = 0;
        uint16_t id = 0;
        uint16_t packet_id = 0;
        bool broadcast = false;
        uint8_t seq_cnt = 0;
    };

    struct AresFrameLoraConfig {
        AresFrameLoraConfig() = default;

        uint32_t frequency = 0;
        uint16_t preamble_length = 0;
        uint8_t bandwidth = 0;
        uint8_t data_rate = 0;
        uint8_t coding_rate = 0;
        int8_t tx_power = 0;
    };

    struct AresFrameLed {
        enum LedState : uint8_t {
            OFF = 0,
            ON = 1,
            BLINK = 2,
            FETCH = 3,
        };

        LedState state = FETCH;
    };

    struct AresFrameHeartbeat {
        AresFrameHeartbeat() = default;

        bool ready = false;
        bool broadcast = false;
        uint8_t tx_cnt = 0;
        uint16_t id = 0;
    };

    struct AresFrameClaim {
        uint16_t id = 0;
    };

    struct AresFrameLog {
        AresFrameLog(bool broadcast, uint8_t tx_cnt, uint16_t id,
                     std::string msg)
            : broadcast(broadcast), tx_cnt(tx_cnt), id(id),
              msg(std::move(msg)) {}
        AresFrameLog() = default;

        bool broadcast = false;
        uint8_t tx_cnt = 1;
        uint8_t part = 1;      // 1 indexed. Automatically managed.
        uint8_t num_parts = 1; // starts at 1. Automatically managed.
        uint16_t id = 0;
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
                                            sizeof(tx_cnt);
    };

    using AresFrameAckErrorCode = int32_t;

    enum AresFrameFramingError : uint8_t {
        BAD_FRAME = 0,
        BAD_TYPE = 1,
        NOT_IMPLEMENTED = 2,
    };

    using AresFrameTxTypes =
        std::variant<std::monostate, AresFrameSetting, AresFrameStart,
                     AresFrameLoraConfig, AresFrameLed, AresFrameHeartbeat,
                     AresFrameClaim, AresFrameLog>;
    using AresFrameRxTypes =
        std::variant<std::monostate, AresFrameSetting, AresFrameStart,
                     AresFrameAckErrorCode, AresFrameFramingError, AresFrameLed,
                     AresFrameHeartbeat, AresFrameClaim, AresFrameLog>;

    using AresFrameResponseTypes =
        std::variant<std::monostate, AresFrameSetting, AresFrameAckErrorCode,
                     AresFrameFramingError, AresFrameLed>;

    struct AresFrameDecoded {
        AresFrameType type;
        AresFrameRxTypes payload;
    };

    explicit AresFrame(AresFrameType type, AresFrameTxTypes payload);
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

    [[nodiscard]] AresFrameDecoded get_parsed_frame() const;

    [[nodiscard]] bool frame_available() const;

  private:
    enum FrameDirection { TX, RX, UNSPECIFIED };
    bool _new_frame = true;

    FrameDirection _direction;
    AresFrameType _type;
    AresFrameTxTypes _tx_payload;
    AresFrameRxTypes _rx_payload;

    [[nodiscard]] uint16_t _payload_size() const;

    void _preprocess_serialize();
    static void _preprocess_log(AresFrameLog &payload);

    static void _serialize_setting(const AresFrameSetting &payload,
                                   std::vector<uint8_t> &buffer);
    static void _serialize_start(const AresFrameStart &payload,
                                 std::vector<uint8_t> &buffer);
    static void _serialize_lora_config(const AresFrameLoraConfig &payload,
                                       std::vector<uint8_t> &buffer);
    static void _serialize_led(const AresFrameLed &payload,
                               std::vector<uint8_t> &buffer);
    static void _serialize_heartbeat(const AresFrameHeartbeat &payload,
                                     std::vector<uint8_t> &buffer);
    static void _serialize_claim(const AresFrameClaim &payload,
                                 std::vector<uint8_t> &buffer);
    static void _serialize_log(const AresFrameLog &payload,
                               std::vector<uint8_t> &buffer);

    void _deserialize_setting(const uint8_t *buf, size_t len);
    void _deserialize_led(const uint8_t *buf, size_t len);
    void _deserialize_start(const uint8_t *buf, size_t len);
    void _deserialize_heartbeat(const uint8_t *buf, size_t len);
    void _deserialize_claim(const uint8_t *buf, size_t len);
    void _deserialize_log(const uint8_t *buf, size_t len);
    void _deserialize_ack(const uint8_t *buf, size_t len);
    void _deserialize_framing_error(const uint8_t *buf, size_t len);
};

#endif // ARES_ARES_FRAME_HPP
