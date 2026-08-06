/**
 * @file serial_driver.cpp
 *
 * @brief
 *
 * @date 8/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/serial-driver/serial_driver.hpp>
#include <ares/pyutil.hpp>

AresLoraConfig::AresLoraConfig(const py::kwargs &kwargs) {
    ares::from_kwargs(kwargs, SP(frequency), SP(preamble_length), SP(bandwidth),
                      SP(datarate), SP(coding_rate), SP(tx_power));
}

AresFrame::Frame AresLoraConfig::generate_frame() const {
    return AresFrame::Frame(
        AresFrame::LORA_CONFIG,
        AresFrame::LoraConfig(frequency, preamble_length, bandwidth, datarate,
                              coding_rate, tx_power, 0, 0, 0, 0));
}

AresSerial::AresSerial(const std::string &port, const py::kwargs &kwargs)
    : _rx_task([this] { _read_serial(); }),
      _processing_task([this] { _process_frames(); }) {
    SerialInternal::SerialAttributes attr;

    std::chrono::milliseconds serial_timeout = 100ms;

    if (kwargs.contains("serial_timeout")) {
        serial_timeout =
            kwargs["serial_timeout"].cast<std::chrono::milliseconds>();
    }

    if (kwargs.contains("rx_period")) {
        _rx_period = kwargs("rx_period").cast<std::chrono::milliseconds>();
    }

    _serial.port(port);
    _serial.baudrate(BAUD_115200);
    _serial.exclusive(true);
    _serial.timeout(serial_timeout);
    _serial.open();
}

AresSerial::~AresSerial() {
    stop_driver();
    _serial.close();
}
