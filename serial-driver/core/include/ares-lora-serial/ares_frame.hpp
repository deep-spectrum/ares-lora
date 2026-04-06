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
        ACK = 4,
        FRAMING_ERROR = 5,
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
        };

        LedState state = OFF;
    };

    using AresFrameAckErrorCode = int32_t;

    enum AresFrameFramingError : uint32_t {
        BAD_FRAME = 0,
        BAD_TYPE = 1,
        NOT_IMPLEMENTED = 2,
    };

    using AresFrameTxTypes =
        std::variant<std::monostate, AresFrameSetting, AresFrameStart,
                     AresFrameLoraConfig, AresFrameLed>;
    using AresFrameRxTypes = std::variant<std::monostate, AresFrameSetting,
                                          AresFrameStart, AresFrameAckErrorCode,
                                          AresFrameFramingError, AresFrameLed>;

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

  private:
    enum FrameDirection { TX, RX, UNSPECIFIED };

    FrameDirection _direction;
    AresFrameType _type;
    AresFrameTxTypes _tx_payload;
    AresFrameRxTypes _rx_payload;

    [[nodiscard]] uint16_t _payload_size() const;

    static void _serialize_setting(const AresFrameSetting &payload,
                                   std::vector<uint8_t> &buffer);
    static void _serialize_start(const AresFrameStart &payload,
                                 std::vector<uint8_t> &buffer);
    static void _serialize_lora_config(const AresFrameLoraConfig &payload,
                                       std::vector<uint8_t> &buffer);
    static void _serialize_led(const AresFrameLed &payload,
                               std::vector<uint8_t> &buffer);

    void _deserialize_setting(const uint8_t *buf, size_t len);
    void _deserialize_led(const uint8_t *buf, size_t len);
    void _deserialize_start(const uint8_t *buf, size_t len);
    void _deserialize_ack(const uint8_t *buf, size_t len);
    void _deserialize_framing_error(const uint8_t *buf, size_t len);
};

#endif // ARES_ARES_FRAME_HPP
