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
#include <ares-lora-serial/util.hpp>
#include <cassert>
#include <chrono>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <sstream>
#include <stdexcept>
#include <type_traits>

using namespace std::chrono_literals;

namespace py = pybind11;

PYBIND11_MODULE(_ares_lora_serial, m, py::mod_gil_not_used()) {
    py::class_<AresSerialConfigs>(m, "_SerialConfigs", "Serial configurations")
        .def(py::init<>())
        .def(py::init<const py::kwargs &>())
        .def_readwrite("port", &AresSerialConfigs::port, "Serial port")
        .def_readwrite("response_timeout", &AresSerialConfigs::response_timeout,
                       "Response timeout")
        .def_readwrite("rx_period", &AresSerialConfigs::rx_period,
                       "Receive period")
        .def_readwrite("serial_timeout", &AresSerialConfigs::serial_timeout,
                       "Serial timeout");

    py::class_<AresSerial>(m, "_AresSerial", "Serial backend")
        .def(py::init<const AresSerialConfigs &>())
        .def("setting_set", &AresSerial::setting_set, py::arg("setting_id"),
             py::arg("value"))
        .def("setting_get", &AresSerial::setting_get, py::arg("setting_id"));

    py::register_local_exception<AresTimeoutError>(m, "AresTimeout",
                                                   PyExc_TimeoutError);
}

AresSerialConfigs::AresSerialConfigs(const py::kwargs &kwargs) {
    from_kwargs(kwargs, SP(port), SP(response_timeout), SP(rx_period),
                SP(start_callback), SP(serial_timeout));
}

AresSerial::AresSerial(const AresSerialConfigs &configs)
    : _response_timeout(configs.response_timeout),
      _rx_period(configs.rx_period) {
    SerialInternal::SerialAttributes attr;

    _serial.port(configs.port);
    _serial.baudrate(BAUD_115200);
    _serial.exclusive(true);
    _serial.timeout(configs.serial_timeout);
    _serial.open();

    _start_callback = configs.start_callback;
}

AresSerial::~AresSerial() { _serial.close(); }

AresSerial::AresResponse AresSerial::_send_frame(AresFrame &frame) {
    std::vector<uint8_t> tx;
    frame.serialize(tx);

    std::unique_lock lock(_serial_lock);
    _serial.write(tx);
    lock.unlock();

    AresResponse response;

    try {
        response = _response_queue.get(_response_timeout);
    } catch (const ares::queue_exception &exc) {
        if (exc.reason() == ares::queue_exception::QUEUE_EMPTY) {
            throw AresTimeoutError(exc.what());
        }
        throw;
    }

    return response;
}

void AresSerial::_handle_bad_frame(const AresResponse &response) {
    std::stringstream ss;
    ss << "Internal error: Bad frame received (code: "
       << std::get<AresFrame::AresFrameFramingError>(response.payload) << ")";
    throw py::buffer_error(ss.str());
}

int AresSerial::setting_set(uint16_t id, uint32_t value) {
    AresFrame frame(AresFrame::SETTING,
                    AresFrame::AresFrameSetting{true, id, value});
    AresResponse response = _send_frame(frame);

    int ret = -1;
    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::AresFrameAckErrorCode>(response.payload);
        break;
    }
    case AresResponse::BAD_FRAME: {
        _handle_bad_frame(response);
        break;
    }
    default: {
        throw std::runtime_error("Received invalid response");
    }
    }

    return ret;
}

py::tuple AresSerial::setting_get(uint16_t id) {
    AresFrame frame(AresFrame::SETTING, AresFrame::AresFrameSetting{false, id});
    AresResponse response = _send_frame(frame);

    int ret = 0;
    uint32_t setting = 0;

    switch (response.type) {
    case AresResponse::COMMAND_SPECIFIC: {
        AresFrame::AresFrameSetting setting_ =
            std::get<AresFrame::AresFrameSetting>(response.payload);
        setting = setting_.value;
        break;
    }
    case AresResponse::ACK: {
        ret = std::get<AresFrame::AresFrameAckErrorCode>(response.payload);
        break;
    }
    default: {
        _handle_bad_frame(response);
    }
    }

    return py::make_tuple(setting, ret);
}

void AresSerial::start() {
    if (_tasks_running || _serial.is_closed()) {
        throw std::runtime_error("Please stop before restarting");
    }

    _tasks_running = true;

    _processing_task.task = std::packaged_task([this] { _process_frames(); });
    _processing_task.future = _processing_task.task.get_future();
    _processing_task.thread = std::thread(std::move(_processing_task.task));

    _rx_task.task = std::packaged_task([this] { _read_serial(); });
    _rx_task.future = _rx_task.task.get_future();
    _rx_task.thread = std::thread(std::move(_rx_task.task));
}

void AresSerial::stop() {
    constexpr size_t max_attempts = 10;
    if (!_tasks_running) {
        return;
    }

    _tasks_running = false;

    while (true) {
        auto status = _rx_task.future.wait_for(10ms);
        if (status == std::future_status::ready) {
            break;
        }
    }
    _rx_task.thread.join();

    size_t retries = 0;
    for (; retries < max_attempts; retries++) {
        size_t attempts = 0;
        for (; attempts < max_attempts; attempts++) {
            auto status = _processing_task.future.wait_for(10ms);
            if (status == std::future_status::ready) {
                break;
            }
        }

        if (attempts >= max_attempts) {
            AresFrame::AresFrameDecoded dummy{AresFrame::UNKNOWN,
                                              std::monostate()};
            _frame_q.put(dummy);
        } else {
            _processing_task.thread.join();
            break;
        }
    }

    if (retries >= max_attempts) {
        throw std::runtime_error("Failed to shut down processing task");
    }
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
            _start_event(std::get<AresFrame::AresFrameStart>(frame.payload));
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
    std::unique_lock lock(_serial_lock, std::defer_lock);

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
            std::this_thread::sleep_for(_rx_period);
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
            if constexpr (std::is_constructible_v<decltype(response.payload),
                                                  T>) {
                response.payload = arg;
            } else {
                assert(false);
            }
        },
        frame.payload);

    _response_queue.put(response);
}

void AresSerial::_start_event(
    const AresFrame::AresFrameStart &start_frame) const {
    if (_start_callback == nullptr) {
        return;
    }

    _start_callback(start_frame.sec, start_frame.nsec, start_frame.id,
                    start_frame.broadcast, start_frame.seq_cnt,
                    start_frame.packet_id);
}
