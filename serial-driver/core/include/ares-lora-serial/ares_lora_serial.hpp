/**
 * @file ares_lora_serial.hpp
 *
 * @brief
 *
 * @date 3/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_ARES_LORA_SERIAL_HPP
#define ARES_ARES_LORA_SERIAL_HPP

#include <serial/serial.hpp>
#include <string>

struct AresSerialConfigs {
    AresSerialConfigs(const char *port, uint32_t baudrate);

    std::string serial_port;
    uint32_t baud;
};

class AresSerial {
  public:
    explicit AresSerial(const AresSerialConfigs &configs);
    ~AresSerial();

    void setting_set(uint16_t id, uint32_t value);
    uint32_t setting_get(uint16_t id);

  private:
    Serial::Serial _serial;
};

#endif // ARES_ARES_LORA_SERIAL_HPP
