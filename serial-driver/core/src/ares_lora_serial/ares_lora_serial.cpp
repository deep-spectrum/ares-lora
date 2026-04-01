/**
 * @file ares_lora_serial.cpp
 *
 * @brief
 *
 * @date 3/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/ares_frame.hpp>
#include <ares-lora-serial/ares_lora_serial.hpp>
#include <chrono>
#include <pybind11/pybind11.h>

using namespace std::chrono_literals;

namespace py = pybind11;

PYBIND11_MODULE(_ares_lora_serial, m, py::mod_gil_not_used()) {
    py::class_<AresSerialConfigs>(m, "_SerialConfigs", "Serial configurations")
        .def(py::init<const char *, uint32_t>());

    py::class_<AresSerial>(m, "_AresSerial", "Serial backend")
        .def(py::init<const AresSerialConfigs &>())
        .def("setting_set", &AresSerial::setting_set, py::arg("setting_id"),
             py::arg("value"))
        .def("setting_get", &AresSerial::setting_get, py::arg("setting_id"));
}

AresSerialConfigs::AresSerialConfigs(const char *port, uint32_t baudrate)
    : serial_port(port), baud(baudrate) {}

AresSerial::AresSerial(const AresSerialConfigs &configs) {
    SerialInternal::SerialAttributes attr;

    _serial.port(configs.serial_port);
    _serial.baudrate(BAUD_115200);
    _serial.exclusive(true);
    _serial.timeout(100ms);
    _serial.open();
}

AresSerial::~AresSerial() { _serial.close(); }

void AresSerial::setting_set(uint16_t id, uint32_t value) {
    AresFrame frame(AresFrame::SETTING,
                    AresFrame::AresFrameSetting{true, id, value});
    std::vector<uint8_t> frame_buf;

    frame.serialize(frame_buf);

    _serial.write(frame_buf);
}

uint32_t AresSerial::setting_get(uint16_t id) {
    AresFrame frame(AresFrame::SETTING, AresFrame::AresFrameSetting{false, id});
    std::vector<uint8_t> frame_buf;

    frame.serialize(frame_buf);

    _serial.write(frame_buf);

    return 0;
}
