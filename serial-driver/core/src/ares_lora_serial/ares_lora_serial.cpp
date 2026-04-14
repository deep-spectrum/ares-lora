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
#include <ares/logging/log.hpp>
#include <ares/util.hpp>
#include <cassert>
#include <chrono>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <sstream>
#include <stdexcept>
#include <type_traits>

LOG_MODULE_REGISTER(serial_logger);

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
        .def("setting_get", &AresSerial::setting_get, py::arg("setting_id"))
        .def("start", &AresSerial::send_start, py::arg("sec"), py::arg("nsec"),
             py::arg("id"), py::arg("broadcast"))
        .def("lora_config", &AresSerial::lora_config, py::arg("config"))
        .def("led", &AresSerial::led, py::arg("state"),
             "Retrieve or set the LED state")
        .def("send_heartbeat", &AresSerial::send_heartbeat, py::arg("ready"),
             py::arg("tx_cnt"), "Send heartbeat packet")
        .def("start_driver", &AresSerial::start, "Start the serial driver")
        .def("stop_driver", &AresSerial::stop, "Stop the serial driver")
        .def("set_response_timeout", &AresSerial::set_response_timeout,
             py::arg("timeout"))
        .def("get_response_timeout", &AresSerial::get_response_timeout);

    py::register_local_exception<AresTimeoutError>(m, "AresTimeout",
                                                   PyExc_TimeoutError);

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

AresSerialConfigs::AresSerialConfigs(const py::kwargs &kwargs) {
    from_kwargs(kwargs, SP(port), SP(response_timeout), SP(rx_period),
                SP(start_callback), SP(serial_timeout), SP(master),
                SP(heartbeat_callback), SP(claim_callback));
}

AresLoraConfig::AresLoraConfig(const py::kwargs &kwargs) {
    from_kwargs(kwargs, SP(frequency), SP(preamble_length), SP(bandwidth),
                SP(datarate), SP(coding_rate), SP(tx_power));
}

AresFrame AresLoraConfig::generate_frame() const {
    return AresFrame(AresFrame::LORA_CONFIG,
                     AresFrame::AresFrameLoraConfig{frequency, preamble_length,
                                                    bandwidth, datarate,
                                                    coding_rate, tx_power});
}

AresSerial::AresSerial(const AresSerialConfigs &configs)
    : _response_timeout(configs.response_timeout),
      _rx_period(configs.rx_period), _rx_task([this] { _read_serial(); }),
      _processing_task([this] { _process_frames(); }), _master(configs.master),
      _heartbeat_work(_heartbeat_handler, this) {
    SerialInternal::SerialAttributes attr;

    _serial.port(configs.port);
    _serial.baudrate(BAUD_115200);
    _serial.exclusive(true);
    _serial.timeout(configs.serial_timeout);
    _serial.open();

    _start_callback = configs.start_callback;
    _heartbeat_callback = configs.heartbeat_callback;
    _claim_callback = configs.claim_callback;
}

AresSerial::~AresSerial() {
    stop();
    _serial.close();
}

AresSerial::AresResponse AresSerial::_send_frame(AresFrame &frame) {
    std::vector<uint8_t> tx;
    frame.serialize(tx);

    _response_queue.clear();

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

int AresSerial::send_start(int64_t sec, uint64_t nsec, uint16_t id,
                           bool broadcast) {
    AresFrame frame(AresFrame::START,
                    AresFrame::AresFrameStart{sec, nsec, id, 0, broadcast, 0});
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

int AresSerial::lora_config(const AresLoraConfig &config) {
    AresFrame frame = config.generate_frame();
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

void AresSerial::set_response_timeout(
    const std::chrono::milliseconds &timeout) {
    _response_timeout = timeout;
}

std::chrono::milliseconds AresSerial::get_response_timeout() const {
    return _response_timeout;
}

py::tuple AresSerial::led(uint8_t state) {
    AresFrame frame(AresFrame::LED,
                    AresFrame::AresFrameLed{
                        static_cast<AresFrame::AresFrameLed::LedState>(state)});
    AresResponse response = _send_frame(frame);

    int ret = 0;
    AresFrame::AresFrameLed val;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::AresFrameAckErrorCode>(response.payload);
        break;
    }
    case AresResponse::BAD_FRAME: {
        _handle_bad_frame(response);
        break;
    }
    case AresResponse::COMMAND_SPECIFIC: {
        val = std::get<AresFrame::AresFrameLed>(response.payload);
        break;
    }
    default: {
        throw std::runtime_error("Received invalid response");
    }
    }

    return py::make_tuple(static_cast<uint8_t>(val.state), ret);
}

int AresSerial::send_heartbeat(bool ready, uint8_t tx_cnt) {
    if (_master) {
        return -EINVAL;
    }

    AresFrame frame(AresFrame::HEARTBEAT,
                    AresFrame::AresFrameHeartbeat{ready,
                                                  _claimed_host == UINT16_C(0),
                                                  tx_cnt, _claimed_host});
    AresResponse response = _send_frame(frame);

    int ret = 0;

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

void AresSerial::start() {
    if (_tasks_running || _serial.is_closed()) {
        throw std::runtime_error("Please stop before restarting");
    }

    _tasks_running = true;
    _processing_task.set_essential(true);
    _processing_task.start();
    _rx_task.set_essential(true);
    _rx_task.start();
}

void AresSerial::stop() {
    constexpr size_t max_attempts = 10;
    if (!_tasks_running) {
        return;
    }

    _tasks_running = false;
    _rx_task.join();

    size_t retries = 0;
    for (; retries < max_attempts; retries++) {
        if (_processing_task.join(100ms) == -ETIMEDOUT) {
            AresFrame::AresFrameDecoded dummy{AresFrame::UNKNOWN,
                                              std::monostate()};
            _frame_q.put(dummy);
            continue;
        }
        break;
    }

    if (retries >= max_attempts) {
        throw std::runtime_error("Failed to shut down processing task");
    }

    _response_queue.clear();
    _frame_q.clear();
}

void AresSerial::_process_frames_helper() {
    while (_tasks_running) {
        AresFrame::AresFrameDecoded frame = _frame_q.get();
        LOG_INF("Received frame: %d", frame.type);

        switch (frame.type) {
        case AresFrame::ACK:
        case AresFrame::FRAMING_ERROR:
        case AresFrame::SETTING:
        case AresFrame::LED: {
            _publish_response(frame);
            break;
        }
        case AresFrame::START: {
            _start_event(std::get<AresFrame::AresFrameStart>(frame.payload));
            break;
        }
        case AresFrame::HEARTBEAT: {
            _heartbeat_event(
                std::get<AresFrame::AresFrameHeartbeat>(frame.payload));
            break;
        }
        case AresFrame::CLAIM: {
            _claim_event(std::get<AresFrame::AresFrameClaim>(frame.payload));
            break;
        }
        default: {
            LOG_ERR("Invalid frame received: %d", static_cast<int>(frame.type));
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
        LOG_DBG("Processing %u bytes", buf.size());
        auto [frame_start, frame_size, _] = AresFrame::frame_present(buf);
        if (frame_start < 0) {
            LOG_DBG("Frame not found");
            return;
        }

        LOG_INF_HEXDUMP(buf, frame_size, "Frame found");
        AresFrame frame;
        frame.parse(buf, frame_start);
        _frame_q.put(frame.get_parsed_frame());
        buf.erase(buf.begin(), buf.begin() + frame_start + frame_size);
    }
}

void AresSerial::_read_serial_helper() {
    std::vector<uint8_t> rx;
    std::unique_lock lock(_serial_lock, std::defer_lock);

    LOG_DBG("Starting RX task");

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
            std::abort();
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
        LOG_DBG("ACK");
        break;
    }
    case AresFrame::FRAMING_ERROR: {
        response.type = AresResponse::BAD_FRAME;
        LOG_DBG("FRAMING ERROR");
        break;
    }
    default: {
        LOG_DBG("COMMAND SPECIFIC");
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

    LOG_INF("Start event received: (%ld, %lu, %u, %d, %d, %u)", start_frame.sec,
            start_frame.nsec, start_frame.id, start_frame.broadcast,
            start_frame.seq_cnt, start_frame.packet_id);

    if (_start_callback == nullptr) {
        LOG_DBG("No callback registered");
        return;
    }

    LOG_DBG("Calling registered callback for start event");

    _start_callback(start_frame.sec, start_frame.nsec, start_frame.id,
                    start_frame.broadcast, start_frame.seq_cnt,
                    start_frame.packet_id);
}

void AresSerial::_heartbeat_event(
    const AresFrame::AresFrameHeartbeat &heartbeat) {
    LOG_INF("Heartbeat received from %d", heartbeat.id);

    if (_master && heartbeat.broadcast) {
        LOG_DBG("Heartbeat received from hanging node");
        _heartbeat_work.sem.lock();
        _heartbeat_work.id = heartbeat.id;
        _work_q.submit(&_heartbeat_work.work);
    }

    if (_heartbeat_callback != nullptr) {
        LOG_DBG("Calling Python event handler");
        _heartbeat_callback(heartbeat.id, heartbeat.ready, heartbeat.broadcast);
    }
}

void AresSerial::_heartbeat_handler(Work *work) {
    HeartbeatWork *hwork = container_of(work, &HeartbeatWork::work);
    uint16_t id = hwork->id;
    hwork->sem.unlock();

    int ret = hwork->obj->_heartbeat_claim_host(id);
    LOG_DBG("Claim host response: %d", ret);
}

int AresSerial::_heartbeat_claim_host(uint16_t destination_id) {
    AresFrame frame(AresFrame::CLAIM,
                    AresFrame::AresFrameClaim{destination_id});
    AresResponse response = _send_frame(frame);
    int ret = -EINVAL;

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
        LOG_ERR("Received invalid response");
        break;
    }
    }

    return ret;
}

void AresSerial::_claim_event(const AresFrame::AresFrameClaim &claim) {
    LOG_DBG("Claim event received: %d", claim.id);
    if (_master) {
        LOG_ERR("Someone sent a claim packet while this is the master node: "
                "Undefined behavior");
        return;
    }

    if (_claimed_host != UINT16_C(0) && claim.id != _claimed_host) {
        LOG_ERR("Someone sent claim packet even though the host has already "
                "been claimed");
        return;
    }

    _claimed_host = claim.id;

    if (_claim_callback != nullptr) {
        _claim_callback(claim.id);
    }
}
