/**
 * @file bindings.cpp
 *
 * @brief
 *
 * @date 8/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/serial-driver/serial_driver.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>

PYBIND11_MODULE(_ares_lora_serial, m, py::mod_gil_not_used()) {
    // AresSerial
    py::class_<AresSerial>(m, "_AresSerial")
        .def(py::init<const std::string &, const py::kwargs &>())
        // Frames
        .def("setting", &AresSerial::setting)
        .def("lora_config", &AresSerial::lora_config, py::arg("config"))
        .def("led", &AresSerial::led)
        .def("version", &AresSerial::version)
        .def("reboot", &AresSerial::reboot)
        .def("start", &AresSerial::start, py::arg("second"), py::arg("usec"))
        .def("poll", &AresSerial::poll)
        .def("log", &AresSerial::log)
        .def("abort", &AresSerial::abort)
        .def("node_config", &AresSerial::node_config)
        .def("notify_run_ready", &AresSerial::notify_run_ready)

        // Events
        .def("wait_start_event", &AresSerial::wait_start_event)
        .def("wait_log_event", &AresSerial::wait_log_event)
        .def("wait_packet_rx_event", &AresSerial::wait_packet_rx_event)
        .def("wait_packet_tx_event", &AresSerial::wait_packet_tx_event)
        .def("wait_abortion_event", &AresSerial::wait_abortion_event)
        .def("wait_ble_connection_event",
             &AresSerial::wait_ble_connection_event)
        .def("wait_ble_subscribe_event", &AresSerial::wait_ble_subscribe_event)
        .def("wait_run_ready_event", &AresSerial::wait_run_ready_event)

        // Driver utilities
        .def("start_driver", &AresSerial::start_driver)
        .def("stop_driver", &AresSerial::stop_driver)
        .def("get_node_config", &AresSerial::get_node_config)
        .def("cancel_events", &AresSerial::cancel_events)

        // Logging utilities
        .def("register_logger", &AresSerial::register_logger_callbacks,
             py::arg("dbg"), py::arg("info"), py::arg("warn"), py::arg("error"),
             py::arg("crit"), py::arg("get_level"), py::arg("set_level"))
        .def("set_logging_level", &AresSerial::set_logging_level,
             py::arg("level"))
        .def("get_logging_level", &AresSerial::get_log_level)

        // Properties
        .def_property("ready", &AresSerial::get_ready, &AresSerial::set_ready);

    // AresLoraConfig
    py::class_<AresLoraConfig>(m, "_AresLoraConfig",
                               "LoRa configurations container")
        .def(py::init<>())
        .def(py::init<const py::kwargs>())
        .def_readwrite("frequency", &AresLoraConfig::frequency,
                       "LoRa center frequency in Hz")
        .def_readwrite("preamble_length", &AresLoraConfig::preamble_length,
                       "Preamble length")
        .def_readwrite("bandwidth", &AresLoraConfig::bandwidth,
                       "LoRa bandwidth")
        .def_readwrite("datarate", &AresLoraConfig::datarate, "LoRa data rate")
        .def_readwrite("coding_rate", &AresLoraConfig::coding_rate,
                       "LoRa coding rate")
        .def_readwrite("tx_power", &AresLoraConfig::tx_power, "LoRa tx power");
}
