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
#include <cassert>
#include <chrono>
#include <pybind11/pybind11.h>
#include <type_traits>

using namespace std::chrono_literals;

namespace py = pybind11;

PYBIND11_MODULE(_ares_lora_serial, m, py::mod_gil_not_used()) {
    py::class_<AresSerialConfigs>(m, "_SerialConfigs", "Serial configurations")
        .def(py::init<const char *, uint32_t, uint32_t>());

    py::class_<AresSerial>(m, "_AresSerial", "Serial backend")
        .def(py::init<const AresSerialConfigs &>())
        .def("setting_set", &AresSerial::setting_set, py::arg("setting_id"),
             py::arg("value"))
        .def("setting_get", &AresSerial::setting_get, py::arg("setting_id"));
}

AresSerialConfigs::AresSerialConfigs(const char *port, uint32_t ack_timeout,
                                     uint32_t rx_period) {
    serial_port = port;
    timeout = ack_timeout;
    this->rx_period = rx_period;
}

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

void AresSerial::_process_frames_helper() {
    while (_tasks_running) {
        AresFrame::AresFrameDecoded frame = _frame_q.get();

        switch (frame.type) {
        case AresFrame::ACK:
        case AresFrame::FRAMING_ERROR:
        case AresFrame::SETTING: {
            _publish_response(frame);
            break;
        }
        case AresFrame::START: {
            // todo
            break;
        }
        default: {
            // todo: Invalid frame received
            break;
        }
        }
    }
}

void AresSerial::_process_frames() {
    while (_tasks_running) {
        try {
            _process_frames_helper();
        } catch (const std::exception &exc) {
            // todo
            std::abort(); // abort for now...
        }
    }
}

void AresSerial::_process_rx_buffer(std::vector<uint8_t> &buf) {
    while (true) {
        auto [frame_start, frame_size, _] = AresFrame::frame_present(buf);
        if (frame_start < 0) {
            return;
        }

        AresFrame frame;
        frame.parse(buf, frame_start);
        _frame_q.put(frame.get_parsed_frame());
        buf.erase(buf.begin(), buf.begin() + frame_start + frame_size);
    }
}

void AresSerial::_read_serial_helper() {
    std::vector<uint8_t> rx;
    std::unique_lock<std::recursive_mutex> lock(_serial_lock, std::defer_lock);

    while (_tasks_running) {
        lock.lock();
        if (_serial.in_waiting() > 0) {
            std::vector<uint8_t> buf;
            _serial.read_all(buf);
            rx.insert(rx.end(), buf.begin(), buf.end());
            lock.unlock();
            _process_rx_buffer(rx);
        } else {
            lock.unlock();
            // todo: sleep
            // std::this_thread::sleep_for()
        }
    }
}

void AresSerial::_read_serial() {
    while (_tasks_running) {
        try {
            _read_serial_helper();
        } catch (const Serial::SerialException &exc) {
            _serial.close();
            // todo
        } catch (const std::exception &exc) {
            // todo
            std::abort();
        }
    }
}

void AresSerial::_publish_response(const AresFrame::AresFrameDecoded &frame) {
    AresResponse response;

    switch (frame.type) {
    case AresFrame::ACK: {
        response.type = AresResponse::ACK;
        break;
    }
    case AresFrame::SETTING: {
        response.type = AresResponse::BAD_FRAME;
        break;
    }
    default: {
        response.type = AresResponse::COMMAND_SPECIFIC;
    }
    }

    std::visit(
        [&response](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_constructible_v<decltype(response.frame),
                                                  T>) {
                response.frame = arg;
            } else {
                assert(false);
            }
        },
        frame.payload);

    _response_queue.put(response);
}

AresSerial::AresResponse AresSerial::_send_frame(AresFrame &frame) {
    std::vector<uint8_t> tx;
    frame.serialize(tx);

    std::unique_lock<std::recursive_mutex> lock(_serial_lock);
    _serial.write(tx);
    lock.unlock();

    return _response_queue.get(); // todo: timeout
}
