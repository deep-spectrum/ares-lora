/**
 * @file lora_config.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LORA_CONFIG_HPP
#define ARES_LORA_CONFIG_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct LoraConfig
 *
 * Data for AresFrame::LORA_CONFIG frames.
 */

struct LoraConfig : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = LORA_CONFIG;

    /**
     * Constructor.
     */
    LoraConfig() = default;

    /**
     * Constructor.
     * @param frequency See LoraConfig::drequency.
     * @param preamble_length See LoraConfig::preamble_length.
     * @param bandwidth See LoraConfig::bandwidth.
     * @param data_rate See LoraConfig::data_rate.
     * @param coding_rate See LoraConfig::coding_rate.
     * @param tx_power See LoraConfig::tx_power.
     * @param cad_mode See LoraConfig::cad_mode.
     * @param cad_num_symbols See LoraConfig::cad_num_symbols.
     * @param cad_det_peak See LoraConfig::cad_det_peak.
     * @param cad_det_min See LoraConfig::cad_det_min.
     */
    explicit LoraConfig(uint32_t frequency, uint16_t preamble_length,
                        uint8_t bandwidth, uint8_t data_rate,
                        uint8_t coding_rate, int8_t tx_power, uint8_t cad_mode,
                        uint8_t cad_num_symbols, uint8_t cad_det_peak,
                        uint8_t cad_det_min)
        : frequency(frequency), preamble_length(preamble_length),
          bandwidth(bandwidth), data_rate(data_rate), coding_rate(coding_rate),
          tx_power(tx_power), cad_mode(cad_mode),
          cad_num_symbols(cad_num_symbols), cad_det_peak(cad_det_peak),
          cad_det_min(cad_det_min)

    {}

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

    /**
     * Payload size.
     * @return The payload size.
     */
    std::size_t payload_size() override;

    /**
     * Encode into a buffer.
     * @param buffer The buffer to place data into.
     */
    void serialize(std::vector<uint8_t> &buffer) override;

  private:
    static constexpr std::size_t _payload_size = 14;
};
} // namespace AresFrame

#endif // ARES_LORA_CONFIG_HPP
