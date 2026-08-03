/**
 * @file ares_lora_serial.cpp
 *
 * @brief
 *
 * @date 3/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/ares_lora_serial.hpp>
#include <ares-lora-serial/frames/frame.hpp>
#include <ares/logging/log.hpp>
#include <ares/pyutil.hpp>
#include <ares/util.hpp>
#include <cassert>
#include <chrono>
#include <pybind11/chrono.h>
// ReSharper disable CppUnusedIncludeDirective
#include <pybind11/functional.h>
// ReSharper restore CppUnusedIncludeDirective
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
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
        .def("start", &AresSerial::send_start, py::arg("sec"), py::arg("usec"),
             py::arg("id"), py::arg("broadcast"), py::arg("ack_timeout"))
        .def("lora_config", &AresSerial::lora_config, py::arg("config"))
        .def("led", &AresSerial::led, py::arg("led_id"), py::arg("state"),
             "Retrieve or set the LED state")
        .def("send_poll", &AresSerial::send_poll, py::arg("id"),
             py::arg("timeout"))
        .def_property("ready", &AresSerial::get_ready, &AresSerial::set_ready)
        .def("send_log", &AresSerial::send_log,
             "Send a logging message over LoRa")
        .def("version", &AresSerial::version, "Retrieve the firmware version")
        .def("ble_state", &AresSerial::ble_state, py::arg("value"))
        .def("ble_disconnect", &AresSerial::ble_disconnect)
        .def("ble_send_image", &AresSerial::ble_send_image, py::arg("image"))
        .def("reboot", &AresSerial::reboot, py::arg("delay"))
        .def("abort", &AresSerial::abort, py::arg("broadcast"), py::arg("id"),
             py::arg("ack_timeout"))
        .def("node_config", &AresSerial::node_config, py::arg("id"),
             py::arg("ack_timeout"))
        .def("poll_node_configs", &AresSerial::node_config_poll, py::arg("id"),
             py::arg("ack_timeout"))
        .def("notify_run_ready", &AresSerial::notify_run_ready,
             py::arg("broadcast"), py::arg("id"), py::arg("timeout"))
        .def("register_logger_callbacks",
             &AresSerial::register_logger_callbacks, py::arg("dbg"),
             py::arg("info"), py::arg("warning"), py::arg("error"),
             py::arg("critical"), py::arg("get_level"), py::arg("set_level"))
        .def("set_logging_level", &AresSerial::set_logging_level,
             "Set the logging level of the C++ logger")
        .def("get_logging_level", &AresSerial::get_log_level)
        .def("start_driver", &AresSerial::start, "Start the serial driver")
        .def("stop_driver", &AresSerial::stop, "Stop the serial driver")
        .def("set_response_timeout", &AresSerial::set_response_timeout,
             py::arg("timeout"))
        .def("get_response_timeout", &AresSerial::get_response_timeout)
        .def("wait_start_event", &AresSerial::wait_start_event)
        .def("wait_poll_event", &AresSerial::wait_poll_event)
        .def("wait_log_event", &AresSerial::wait_log_event)
        .def("wait_packet_rx_event", &AresSerial::wait_packet_rx_event)
        .def("wait_packet_tx_done_event",
             &AresSerial::wait_packet_tx_done_event)
        .def("wait_ble_connect_event", &AresSerial::wait_ble_connection_event)
        .def("wait_ble_subscribe_event",
             &AresSerial::wait_ble_subscription_event)
        .def("wait_abort_event", &AresSerial::wait_abort_event)
        .def("wait_node_ready_event", &AresSerial::wait_node_ready_event)
        .def("cancel_events", &AresSerial::cancel_events)
        .def_property_readonly("node_configs", &AresSerial::get_node_config);

    py::register_local_exception<AresTimeoutError>(m, "AresTimeout",
                                                   PyExc_TimeoutError);

    py::register_local_exception<AresThreadTerminate>(m, "AresThreadTerminate",
                                                      PyExc_Exception);

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
                SP(serial_timeout), SP(alpha), SP(beta));
}

AresLoraConfig::AresLoraConfig(const py::kwargs &kwargs) {
    from_kwargs(kwargs, SP(frequency), SP(preamble_length), SP(bandwidth),
                SP(datarate), SP(coding_rate), SP(tx_power));
}

AresFrame::Frame AresLoraConfig::generate_frame() const {
    return AresFrame::Frame(
        AresFrame::LORA_CONFIG,
        AresFrame::LoraConfig{frequency, preamble_length, bandwidth, datarate,
                              coding_rate, tx_power, 0, 0, 0, 0});
}

AresSerial::AresSerial(const AresSerialConfigs &configs)
    : _response_timeout(configs.response_timeout),
      _rx_period(configs.rx_period), _rx_task([this] { _read_serial(); }),
      _processing_task([this] { _process_frames(); }),
      _lora_ack_work(_lora_msg_ack_handler, this),
      _heartbeat_work(_heartbeat_handler, this),
      _mac_backoff(configs.alpha, configs.beta), _generator(_rd()),
      _node_response_work(_node_config_response_work_handler, this) {
    SerialInternal::SerialAttributes attr;

    _serial.port(configs.port);
    _serial.baudrate(BAUD_115200);
    _serial.exclusive(true);
    _serial.timeout(configs.serial_timeout);
    _serial.open();
}

AresSerial::~AresSerial() {
    stop();
    _serial.close();
}

void AresSerial::_send_frame(const std::vector<uint8_t> &tx) {
    LOG_DBG_HEXDUMP(tx, tx.size(), "Sending frame");
    std::unique_lock lock(_serial_lock);
    _serial.write(tx);
}

AresSerial::AresResponse
AresSerial::_send_frame(AresFrame::Frame &frame,
                        const std::chrono::milliseconds &timeout) {
    py::gil_scoped_release release;
    return _send_frame_released(frame, timeout);
}

AresSerial::AresResponse
AresSerial::_send_frame_released(AresFrame::Frame &frame,
                                 const std::chrono::milliseconds &timeout) {
    std::unique_lock lock_(_command_lock);
    std::vector<uint8_t> tx;
    frame.serialize(tx);

    _response_queue.clear();
    _send_frame(tx);
    return _wait_response(timeout);
}

static void check_python_errors() {
    py::gil_scoped_acquire acquire;

    if (PyErr_CheckSignals() != 0) {
        throw py::error_already_set();
    }
}

void AresSerial::_send_multi_frame(AresFrame::Frame &frame,
                                   const std::chrono::milliseconds &timeout,
                                   std::vector<AresResponse> &responses) {
    do {
        AresResponse response = _send_frame(frame, timeout);
        responses.emplace_back(response);
        check_python_errors();
    } while (frame.frame_available());
}

void AresSerial::_send_log_frame_directed(
    AresFrame::Frame &frame, const std::chrono::milliseconds &ack_timeout,
    size_t max_attempts, std::vector<AresResponse> &responses,
    uint16_t target) {
    py::gil_scoped_release release;
    std::vector<uint8_t> tx;
    size_t acked_frames = 0;

    do {
        acked_frames++;
        frame.serialize(tx);
        size_t tot_frames = frame.total_frames();
        AresResponse last_response;
        bool acked = false;

        for (size_t attempt = 0u; attempt < max_attempts && !acked; attempt++) {
            _response_queue.clear();
            _send_frame(tx);
            last_response = _wait_response(_response_timeout);
            check_python_errors();
            acked = _log_ack_event_wait(ack_timeout, acked_frames, tot_frames,
                                        target);
            _handle_ack(target, acked);
        }
        responses.emplace_back(last_response);

        if (!acked) {
            throw std::runtime_error("Did not receive an ACK");
        }
    } while (frame.frame_available());
}

void AresSerial::_handle_ack(uint16_t target, bool acked) {
    check_python_errors();

    if (acked) {
        return;
    }

    py::tuple ret = setting_get(0);
    uint16_t id = static_cast<uint16_t>(ret[0].cast<uint32_t>());
    constexpr double min_delay = 100.0;
    const double lambda = 5.0 / static_cast<double>(id + target);
    double delay = _mac_backoff(_generator);
    delay = ((-std::log(1.0 - delay)) / lambda) + min_delay;
    auto sleep_for = delay * 1ms;

    LOG_DBG("ACK not received. Sleeping for %f ms", delay);
    std::this_thread::sleep_for(sleep_for);
}

AresSerial::AresResponse
AresSerial::_wait_response(const std::chrono::milliseconds &timeout) {
    AresResponse response;
    if (timeout == std::chrono::milliseconds::max()) {
        response = _wait_response_forever();
    } else {
        response = _wait_response_timeout(timeout);
    }
    return response;
}

AresSerial::AresResponse
AresSerial::_wait_response_timeout(const std::chrono::milliseconds &timeout) {
    AresResponse response;

    try {
        response = _response_queue.get(timeout);
    } catch (const ares::queue_exception &exc) {
        if (exc.reason() == ares::queue_exception::QUEUE_TIMEOUT) {
            throw AresTimeoutError(exc.what());
        }
        throw;
    }

    return response;
}

AresSerial::AresResponse AresSerial::_wait_response_forever() {
    return _response_queue.get();
}

void AresSerial::_handle_bad_frame(const AresResponse &response) {
    std::stringstream ss;
    ss << "Internal error: Bad frame received (code: "
       << std::get<AresFrame::FramingError>(response.payload).type << ")";
    throw py::buffer_error(ss.str());
}

int AresSerial::setting_set(uint16_t id, uint32_t value) {
    _check_crash();
    AresFrame::Frame frame(AresFrame::SETTING, AresFrame::Setting{id, value});
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = -1;
    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
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
    _check_crash();
    AresFrame::Frame frame(AresFrame::SETTING, AresFrame::Setting{id});
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = 0;
    uint32_t setting = 0;

    switch (response.type) {
    case AresResponse::COMMAND_SPECIFIC: {
        AresFrame::Setting setting_ =
            std::get<AresFrame::Setting>(response.payload);
        setting = setting_.value;
        break;
    }
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
        break;
    }
    default: {
        _handle_bad_frame(response);
    }
    }

    return py::make_tuple(setting, ret);
}

int AresSerial::send_start(int64_t sec, uint64_t usec, uint16_t id,
                           bool broadcast,
                           const std::chrono::seconds &ack_timeout) {
    _check_crash();

    if (!broadcast) {
        py::gil_scoped_release release;
        std::unique_lock lock(_ack_sem);
        _expected_ack_id = id;
    }

    AresFrame::Frame frame(AresFrame::START,
                           AresFrame::Start{sec, usec, id, 0, broadcast, 0});
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = -1;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
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

    if (!broadcast) {
        _wait_lora_ack(id, ack_timeout, AresFrame::START);
    }

    return ret;
}

int AresSerial::lora_config(const AresLoraConfig &config) {
    _check_crash();
    AresFrame::Frame frame = config.generate_frame();
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = -1;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
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

py::tuple AresSerial::led(uint8_t id, uint8_t state) {
    _check_crash();
    AresFrame::Frame frame(
        AresFrame::LED,
        AresFrame::Led{id, static_cast<AresFrame::Led::LedState>(state)});
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = 0;
    AresFrame::Led val;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
        break;
    }
    case AresResponse::BAD_FRAME: {
        _handle_bad_frame(response);
        break;
    }
    case AresResponse::COMMAND_SPECIFIC: {
        val = std::get<AresFrame::Led>(response.payload);
        break;
    }
    default: {
        throw std::runtime_error("Received invalid response");
    }
    }

    return py::make_tuple(static_cast<uint8_t>(val.state), ret);
}

py::tuple AresSerial::send_poll(uint16_t id,
                                const std::chrono::seconds &timeout) {
    _check_crash();
    AresFrame::Frame frame{AresFrame::POLL, AresFrame::Poll{id}};

    {
        std::unique_lock lock(_heartbeat_sem);
        _expected_heartbeat_id = id;
    }

    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = -1;
    bool ready;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
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

    ready = _wait_heartbeat(id, timeout);

    return py::make_tuple(ready, ret);
}

void AresSerial::set_ready(bool new_state) {
    py::gil_scoped_release release;
    std::unique_lock lock(_heartbeat_work.sem);
    _heartbeat_work.ready = new_state;
}

bool AresSerial::get_ready() {
    py::gil_scoped_release release;
    std::unique_lock lock(_heartbeat_work.sem);
    return _heartbeat_work.ready;
}

py::tuple AresSerial::send_log(const std::string &log_msg, bool broadcast,
                               uint8_t tx_cnt, uint16_t id) {
    std::unique_lock lock_(_log_spinlock);
    _check_crash();
    LOG_DBG("Log command received");
    AresFrame::Log payload{broadcast,
                           (broadcast) ? tx_cnt : static_cast<uint8_t>(1), id,
                           _log_id, log_msg};
    _log_id++;

    if (!broadcast && id == UINT16_C(0)) {
        LOG_ERR("Tried sending to invalid ID.");
        throw std::invalid_argument("Bad ID");
    }

    AresFrame::Frame frame{AresFrame::LOG, payload};
    std::vector<AresResponse> responses;

    if (payload.broadcast) {
        _send_multi_frame(frame, _response_timeout, responses);
    } else {
        _send_log_frame_directed(frame, _response_timeout, tx_cnt, responses,
                                 payload.id);
    }

    std::vector<int> ret;

    for (auto &response : responses) {
        switch (response.type) {
        case AresResponse::ACK: {
            ret.emplace_back(std::get<AresFrame::Ack>(response.payload).code);
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
    }

    return ares::array_to_tuple(ret.data(), ret.size());
}

py::tuple AresSerial::version() {
    _check_crash();
    LOG_DBG("Version command received");
    AresFrame::Frame frame(AresFrame::VERSION, AresFrame::Version{});
    AresResponse response = _send_frame(frame, _response_timeout);
    AresFrame::Version version;

    switch (response.type) {
    case AresResponse::COMMAND_SPECIFIC: {
        version = std::get<AresFrame::Version>(response.payload);
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

    return py::make_tuple(_decode_version(version.app),
                          _decode_version(version.ncs),
                          _decode_version(version.kernel));
}

py::tuple AresSerial::ble_state(uint8_t value) {
    _check_crash();
    LOG_DBG("BLE state command received");
    AresFrame::Frame frame(AresFrame::BLE_STATE, AresFrame::BleState(value));
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = 0;
    AresFrame::BleState val;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
        break;
    }
    case AresResponse::BAD_FRAME: {
        _handle_bad_frame(response);
        break;
    }
    case AresResponse::COMMAND_SPECIFIC: {
        val = std::get<AresFrame::BleState>(response.payload);
        break;
    }
    default: {
        throw std::runtime_error("Received invalid response");
    }
    }

    return py::make_tuple(static_cast<uint8_t>(val.state), ret);
}

int AresSerial::ble_disconnect() {
    _check_crash();
    LOG_DBG("BLE state command received");
    AresFrame::Frame frame(AresFrame::BLE_DISCONNECT, std::monostate());
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = -1;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
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

py::tuple AresSerial::ble_send_image(const py::bytes &image) {
    LOG_DBG("Attempting to transfer image");
    py::buffer_info info(py::buffer(image).request());
    auto *data = static_cast<const uint8_t *>(info.ptr);
    auto size = static_cast<size_t>(info.size);
    std::vector image_(data, data + size);

    int ret =
        _ble_send_chunk(AresFrame::BleImage::num_chunks(image_, ble_info.mtu));

    if (ret != 0) {
        return py::make_tuple(ret);
    }

    return _ble_send_image(image_);
}

int AresSerial::reboot(uint8_t delay) {
    LOG_DBG("Reboot command received.");
    AresFrame::Frame frame{AresFrame::REBOOT, AresFrame::Reboot{delay}};
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = -1;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
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

    if (ret == 0) {
        stop();
        _serial.close();
        _wait_until_reboot_done(delay);
    }

    return ret;
}

int AresSerial::abort(bool broadcast, uint16_t id,
                      const std::chrono::seconds &ack_timeout) {
    _check_crash();

    if (!broadcast) {
        py::gil_scoped_release release;
        std::unique_lock lock(_ack_sem);
        _expected_ack_id = id;
    }

    AresFrame::Frame frame{AresFrame::ABORT, AresFrame::Abort{broadcast, id}};
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = -1;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
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

    if (!broadcast) {
        _wait_lora_ack(id, ack_timeout, AresFrame::ABORT);
    }

    return ret;
}

py::dict AresSerial::node_config(uint16_t id,
                                 const std::chrono::seconds &timeout,
                                 const py::kwargs &kwargs) {
    _check_crash();

    {
        py::gil_scoped_release release;
        std::unique_lock lock(_ack_sem);
        _expected_ack_id = id;
    }

    std::vector<AresFrame::Frame> config_frames;
    _parse_node_config_kwargs(id, config_frames, kwargs);

    return _send_node_config_frames(id, config_frames, timeout);
}

py::dict AresSerial::node_config_poll(uint16_t id,
                                      const std::chrono::seconds &timeout,
                                      const py::args &args) {
    _check_crash();

    {
        py::gil_scoped_release release;
        std::unique_lock lock(_node_config_response_sem);
        _expected_node_config_response_id = id;
    }

    std::vector<AresFrame::Frame> config_poll_frames;
    _parse_node_config_poll_args(id, config_poll_frames, args);

    return _send_node_config_poll_frames(id, config_poll_frames, timeout);
}

int AresSerial::notify_run_ready(bool broadcast, uint16_t id,
                                 const std::chrono::seconds &timeout) {
    _check_crash();

    if (!broadcast) {
        py::gil_scoped_release release;
        std::unique_lock lock(_ack_sem);
        _expected_ack_id = id;
    }

    AresFrame::Frame frame(AresFrame::NODE_READY,
                           AresFrame::NodeReady{broadcast, id});
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = -1;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
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

    if (!broadcast) {
        _wait_lora_ack(id, timeout, AresFrame::NODE_READY);
    }

    return ret;
}

void AresSerial::register_logger_callbacks(
    const std::function<void(const std::string &)> &dbg,
    const std::function<void(const std::string &)> &info,
    const std::function<void(const std::string &)> &warn,
    const std::function<void(const std::string &)> &error,
    const std::function<void(const std::string &)> &crit,
    const std::function<long()> &get_level,
    const std::function<void(long)> &set_level) {
    _check_crash();

    LOG_MODULE_REGISTER_CALLBACKS(dbg, info, warn, error, crit, set_level,
                                  get_level);
}

void AresSerial::set_logging_level(uint32_t level) {
    _check_crash();

    switch (level) {
    case 10: {
        SET_LOG_LEVEL(LOG_LEVEL_DBG);
        break;
    }
    case 20: {
        SET_LOG_LEVEL(LOG_LEVEL_INFO);
        break;
    }
    case 30: {
        SET_LOG_LEVEL(LOG_LEVEL_WARN);
        break;
    }
    case 40: {
        SET_LOG_LEVEL(LOG_LEVEL_ERROR);
        break;
    }
    case 50: {
        SET_LOG_LEVEL(LOG_LEVEL_CRITICAL);
        break;
    }
    case 60: {
        SET_LOG_LEVEL(LOG_LEVEL_OFF);
        break;
    }
    default: {
        throw std::invalid_argument("Invalid logging level");
    }
    }
}

long AresSerial::get_log_level() {
    _check_crash();

    return static_cast<long>(LOG_MODULE_CURRENT_LEVEL);
}

void AresSerial::start() {
    if (_tasks_running) {
        throw std::runtime_error("Please stop before restarting");
    }

    LOG_INF("Starting driver");

    LOG_DBG("Clearing event queues");
    _start_event_q.clear();
    _heartbeat_event_q.clear();
    _poll_event_q.clear();
    _log_event_q.clear();
    _pkt_rx_event_q.clear();
    _pkt_tx_event_q.clear();

    _exception = nullptr;
    if (_serial.is_closed()) {
        LOG_DBG("Port was closed. Attempting to open it.");
        _serial.open();
    }

    LOG_DBG("Starting Work Queue");
    ares::WorkQConfig config = {
        .name = "AresSerial queue",
        .essential = true,
    };
    _work_q.start(&config);

    LOG_DBG("Starting Processing Task");
    _tasks_running = true;
    _processing_task.set_essential(true);
    _processing_task.start();

    LOG_DBG("Starting RX Task");
    _rx_task.set_essential(true);
    _rx_task.start();
}

void AresSerial::stop() {
    constexpr size_t max_attempts = 10;
    if (!_tasks_running && !_exception) {
        return;
    }

    LOG_INF("Stopping driver");
    _work_q.queue_drain(true);
    _work_q.stop();

    _tasks_running = false;
    _rx_task.join();

    AresFrame::Frame terminate_request{AresFrame::DRIVER_STOP,
                                       std::monostate()};
    size_t retries = 0;

    do {
        _frame_q.put(terminate_request);
        if (_processing_task.join(100ms) == 0) {
            break;
        }
        retries++;
    } while (retries < max_attempts);

    if (retries >= max_attempts) {
        throw std::runtime_error("Failed to shut down processing task");
    }

    _response_queue.clear();
    _frame_q.clear();
}

template <typename Event, size_t size, bool overwrite>
static void wait_event_queue_released(
    Event &evt,
    ares::bounded_queue<std::unique_ptr<Event>, size, overwrite> &evt_q) {
    py::gil_scoped_release release;

    auto event_ptr = evt_q.get();
    if (event_ptr == nullptr) {
        throw AresThreadTerminate();
    }

    evt = *event_ptr;
}

py::tuple AresSerial::wait_start_event() {
    AresFrame::Start event;
    wait_event_queue_released(event, _start_event_q);
    return py::make_tuple(event.sec, event.usec, event.id, event.broadcast,
                          event.seq_cnt, event.packet_id);
}

uint16_t AresSerial::wait_poll_event() {
    AresFrame::Poll event;
    wait_event_queue_released(event, _poll_event_q);
    return event.id;
}

py::tuple AresSerial::wait_log_event() {
    AresFrame::Log event;
    wait_event_queue_released(event, _log_event_q);
    return py::make_tuple(event.id, event.log_id, event.part, event.num_parts,
                          event.msg);
}

py::tuple AresSerial::wait_packet_rx_event() {
    AresFrame::PktRx event;
    wait_event_queue_released(event, _pkt_rx_event_q);
    return py::make_tuple(event.seq_cnt, event.packet_id, event.src_id);
}

uint32_t AresSerial::wait_packet_tx_done_event() {
    AresFrame::PktTx event;
    wait_event_queue_released(event, _pkt_tx_event_q);
    return event.count;
}

bool AresSerial::wait_ble_connection_event() {
    AresFrame::BleConnect event;
    wait_event_queue_released(event, _ble_connect_event_q);
    return event.connected;
}

py::tuple AresSerial::wait_ble_subscription_event() {
    AresFrame::BleSubscribed event;
    wait_event_queue_released(event, _ble_subscribe_event_q);
    return py::make_tuple(event.chunk, event.image);
}

py::tuple AresSerial::wait_abort_event() {
    AresFrame::Abort event;
    wait_event_queue_released(event, _abort_event_q);
    return py::make_tuple(event.id, event.broadcast);
}

py::tuple AresSerial::wait_node_ready_event() {
    AresFrame::NodeReady event;
    wait_event_queue_released(event, _node_ready_event_q);
    return py::make_tuple(event.id, event.broadcast);
}

py::dict AresSerial::get_node_config() {
    NodeConfigs copy;
    _get_node_configs_released(copy);

    py::dict ret;
    ret["folder_dt"] = copy.save_folder.time_point();
    ret["bandwidth"] = copy.bandwidth;
    ret["center_freq"] = copy.center_freq;
    ret["duration"] = copy.duration;
    ret["ref_level"] = copy.ref_level;

    return ret;
}

void AresSerial::cancel_events() {
    LOG_DBG("Cancelling event queues");
    _stop_event_queues();
}

void AresSerial::_lora_msg_ack_handler(ares::Work *work) {
    LoraAckWork *lwork = ares::container_of(work, &LoraAckWork::work);
    uint16_t id = lwork->id;
    AresFrame::AresFrameType message = lwork->acked_message;
    lwork->sem.give();
    lwork->obj->_send_lora_ack(id, message);
}

void AresSerial::_send_lora_ack(uint16_t id,
                                AresFrame::AresFrameType acked_message) {
    AresFrame::Frame frame(AresFrame::LORA_ACK,
                           AresFrame::LoraAck{id, acked_message});

    AresResponse response;

    try {
        response =
            _send_frame_released(frame, std::chrono::milliseconds::max());
    } catch (const std::exception &e) {
        LOG_ERR("_send_frame_released(): %s", e.what());
        return;
    }

    switch (response.type) {
    case AresResponse::ACK: {
        LOG_DBG("Send LoRa ACK ACK'ed: %d",
                std::get<AresFrame::Ack>(response.payload).code);
        break;
    }
    case AresResponse::BAD_FRAME: {
        LOG_ERR("Bad frame response received in Lora ACK handler");
        break;
    }
    default: {
        LOG_ERR("Received invalid response");
        break;
    }
    }
}

void AresSerial::_wait_lora_ack(uint16_t id,
                                const std::chrono::seconds &timeout,
                                AresFrame::AresFrameType expected_message) {
    py::gil_scoped_release release;
    AresFrame::LoraAck ack;
    bool timed_out = false;
    auto now = std::chrono::steady_clock::now;

    auto to_time = now() + timeout;
    while ((id != ack.id || expected_message != ack.message_type) &&
           !timed_out) {
        try {
            ack = *_lora_ack_q.get(
                std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
        } catch (const ares::queue_exception &e) {
            timed_out = e.reason() == ares::queue_exception::QUEUE_TIMEOUT;
            continue;
        }

        timed_out = now() > to_time;
    }

    if (timed_out) {
        throw AresTimeoutError("Timed out waiting for an acknowledgement");
    }
}

template <typename T, size_t size>
static void put_no_except(const T &item,
                          ares::bounded_queue<std::unique_ptr<T>, size> &q,
                          const std::chrono::milliseconds &timeout) {
    try {
        q.put(std::make_unique<T>(item), timeout);
    } catch (const ares::queue_exception &) {
        // nop
    }
}

void AresSerial::_lora_ack_event(const AresFrame::LoraAck &ack) {
    LOG_INF("ACK received from %d", ack.id);
    {
        std::unique_lock lock(_ack_sem);
        if (ack.id != _expected_ack_id) {
            LOG_WRN("Late ACK received or spurious ACK received");
            return;
        }
    }

    put_no_except(ack, _lora_ack_q, 100ms);
}

void AresSerial::_check_crash() {
    if (_exception) {
        stop();
        std::rethrow_exception(_exception);
    }
}

void AresSerial::_wait_until_reboot_done(uint8_t delay) {
    constexpr uint8_t min = 5;
    constexpr uint8_t max = 30;
    constexpr int added_time = 1;
    py::gil_scoped_release release;
    int delay__;

    if (delay < min) {
        delay__ = min;
    } else if (delay > max) {
        delay__ = max;
    } else {
        delay__ = delay;
    }

    delay__ += added_time;

    const auto delay_ = std::chrono::seconds{delay__};

    std::this_thread::sleep_for(delay_);
}

void AresSerial::_process_frames_helper() {
    bool stopped = false;
    while (_tasks_running || !stopped) {
        AresFrame::Frame frame = _frame_q.get();
        LOG_DBG("Received frame: %d", frame.type());

        switch (frame.type()) {
        case AresFrame::ACK:
        case AresFrame::FRAMING_ERROR:
        case AresFrame::SETTING:
        case AresFrame::LED:
        case AresFrame::VERSION:
        case AresFrame::BLE_STATE: {
            _publish_response(frame);
            break;
        }
        case AresFrame::START: {
            _start_event(std::get<AresFrame::Start>(frame.rx_payload()));
            break;
        }
        case AresFrame::HEARTBEAT: {
            _heartbeat_event(
                std::get<AresFrame::Heartbeat>(frame.rx_payload()));
            break;
        }
        case AresFrame::POLL: {
            _poll_event(std::get<AresFrame::Poll>(frame.rx_payload()));
            break;
        }
        case AresFrame::LOG: {
            _log_event(std::get<AresFrame::Log>(frame.rx_payload()));
            break;
        }
        case AresFrame::LOG_ACK: {
            _log_ack_event(std::get<AresFrame::LogAck>(frame.rx_payload()));
            break;
        }
        case AresFrame::DBG: {
            _debug_event(std::get<AresFrame::Dbg>(frame.rx_payload()));
            break;
        }
        case AresFrame::PKT_RX: {
            _packet_rx_event(std::get<AresFrame::PktRx>(frame.rx_payload()));
            break;
        }
        case AresFrame::PKT_TX: {
            _packet_tx_event(std::get<AresFrame::PktTx>(frame.rx_payload()));
            break;
        }
        case AresFrame::BLE_CONNECTED: {
            _ble_connect_event(
                std::get<AresFrame::BleConnect>(frame.rx_payload()));
            break;
        }
        case AresFrame::BLE_SUBSCRIBED: {
            _ble_subscribe_event(
                std::get<AresFrame::BleSubscribed>(frame.rx_payload()));
            break;
        }
        case AresFrame::LORA_ACK: {
            _lora_ack_event(std::get<AresFrame::LoraAck>(frame.rx_payload()));
            break;
        }
        case AresFrame::ABORT: {
            _abort_event(std::get<AresFrame::Abort>(frame.rx_payload()));
            break;
        }
        case AresFrame::NODE_CONFIG: {
            _handle_node_config_event(
                std::get<AresFrame::NodeConfig>(frame.rx_payload()));
            break;
        }
        case AresFrame::NODE_CONFIG_POLL: {
            _handle_node_config_poll_event(
                std::get<AresFrame::NodeConfigPoll>(frame.rx_payload()));
            break;
        }
        case AresFrame::NODE_CONFIG_RESP: {
            _handle_node_config_response_event(
                std::get<AresFrame::NodeConfigResponse>(frame.rx_payload()));
            break;
        }
        case AresFrame::NODE_READY: {
            _handle_node_ready_event(
                std::get<AresFrame::NodeReady>(frame.rx_payload()));
            break;
        }
        case AresFrame::DRIVER_STOP: {
            stopped = _stop_event_queues();
            break;
        }
        default: {
            LOG_ERR("Invalid frame received: %d",
                    static_cast<int>(frame.type()));
            break;
        }
        }
    }
}

void AresSerial::_process_frames() {
    LOG_DBG("Starting processing task");
    while (_tasks_running) {
        try {
            _process_frames_helper();
        } catch (...) {
            _exception = std::current_exception();
            _tasks_running = false;
            LOG_ERR("Driver crashed in processing thread");
        }
    }
}

void AresSerial::_process_rx_buffer(std::vector<uint8_t> &buf) {
    while (true) {
        LOG_DBG("Processing %u bytes", buf.size());
        auto [frame_start, frame_size, _] =
            AresFrame::Frame::frame_present(buf);
        if (frame_start < 0) {
            LOG_DBG("Frame not found");
            return;
        }

        LOG_DBG_HEXDUMP(buf, frame_size, "Frame found");
        AresFrame::Frame frame;
        frame.parse(buf, frame_start);
        _frame_q.put(frame);
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
        } catch (...) {
            _exception = std::current_exception();
            _serial.close();
            _tasks_running = false;
            LOG_ERR("Driver crashed in read serial task");
        }
    }
}

void AresSerial::_publish_response(const AresFrame::Frame &frame) {
    AresResponse response;

    switch (frame.type()) {
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
        frame.rx_payload());

    _response_queue.put(response);
}

void AresSerial::_start_event(const AresFrame::Start &start_frame) {

    LOG_INF("Start event received: (%ld, %lu, %u, %d, %d, %u)", start_frame.sec,
            start_frame.usec, start_frame.id, start_frame.broadcast,
            start_frame.seq_cnt, start_frame.packet_id);

    if (!start_frame.broadcast) {
        _lora_ack_work.sem.take();
        _lora_ack_work.id = start_frame.id;
        _lora_ack_work.acked_message = AresFrame::START;
        _work_q.submit(&_lora_ack_work.work);
    }

    put_no_except(start_frame, _start_event_q, 100ms);
}

bool AresSerial::_wait_heartbeat(uint16_t id,
                                 const std::chrono::seconds &timeout) {
    py::gil_scoped_release release;
    AresFrame::Heartbeat heartbeat;
    bool timed_out = false;
    auto now = std::chrono::steady_clock::now;

    auto to_time = now() + timeout;
    while (id != heartbeat.id && !timed_out) {
        try {
            heartbeat = *_heartbeat_event_q.get(
                std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
        } catch (const ares::queue_exception &e) {
            timed_out = e.reason() == ares::queue_exception::QUEUE_TIMEOUT;
            continue;
        }

        timed_out = now() > to_time;
    }

    if (timed_out) {
        throw AresTimeoutError("Timed out waiting for a heartbeat response");
    }

    return heartbeat.ready;
}

void AresSerial::_heartbeat_event(const AresFrame::Heartbeat &heartbeat) {
    LOG_INF("Heartbeat received from %d", heartbeat.id);
    {
        std::unique_lock lock(_heartbeat_sem);
        if (heartbeat.id != _expected_heartbeat_id) {
            LOG_WRN("Late heartbeat received or spurious heartbeat received");
            return;
        }
    }

    put_no_except(heartbeat, _heartbeat_event_q, 100ms);
}

void AresSerial::_heartbeat_handler(ares::Work *work) {
    HeartbeatWork *hwork = ares::container_of(work, &HeartbeatWork::work);
    uint16_t id = hwork->id;
    bool ready = hwork->ready;
    hwork->sem.unlock();
    hwork->obj->_send_heartbeat(id, ready);
}

// ReSharper disable once CppDFAUnreachableFunctionCall
void AresSerial::_send_heartbeat(uint16_t id, bool ready) {
    AresFrame::Frame frame{
        AresFrame::HEARTBEAT,
        AresFrame::Heartbeat{ready, id},
    };
    AresResponse response;

    try {
        response =
            _send_frame_released(frame, std::chrono::milliseconds::max());
    } catch (const std::exception &e) {
        LOG_ERR("_send_frame_released(): %s", e.what());
        return;
    }

    switch (response.type) {
    case AresResponse::ACK: {
        LOG_DBG("Send heartbeat ACK'ed: %d",
                std::get<AresFrame::Ack>(response.payload).code);
        break;
    }
    case AresResponse::BAD_FRAME: {
        LOG_ERR("Bad frame response received in heartbeat handler");
        break;
    }
    default: {
        LOG_ERR("Received invalid response");
        break;
    }
    }
}

void AresSerial::_poll_event(const AresFrame::Poll &poll) {
    _heartbeat_work.sem.lock();
    _heartbeat_work.id = poll.id;
    _work_q.submit(&_heartbeat_work.work);
    put_no_except(poll, _poll_event_q, 100ms);
}

void AresSerial::_log_ack_event(const AresFrame::LogAck &ack) {
    LOG_INF("Log ACK event received (%d, %d, %d)", ack.part, ack.num_parts,
            ack.id);

    try {
        _log_ack_signal.put_nonblocking(ack);
    } catch (const ares::queue_exception &exc) {
        if (exc.reason() != ares::queue_exception::QUEUE_FULL) {
            throw;
        }
        LOG_ERR("Signal queue full");
        _log_ack_signal.clear();
    }
}

bool AresSerial::_log_ack_event_wait(const std::chrono::milliseconds &timeout,
                                     size_t part, size_t num_parts,
                                     uint16_t id) {
    AresFrame::LogAck expected{static_cast<uint8_t>(part),
                               static_cast<uint8_t>(num_parts), id};
    AresFrame::LogAck response{};

    _log_ack_signal.clear();
    try {
        response = _log_ack_signal.get(timeout);
    } catch ([[maybe_unused]] const ares::queue_exception &exc) {
        // nop
    }

    LOG_DBG("Expected: (%d, %d, %d)", expected.part, expected.num_parts,
            expected.id);
    LOG_DBG("Received: (%d, %d, %d)", response.part, response.num_parts,
            response.id);
    LOG_DBG("Comparison: %d", response == expected);

    return response == expected;
}

void AresSerial::_log_event(const AresFrame::Log &log) {
    LOG_INF("Log event received from %d", log.id);
    LOG_DBG("Part %d of %d", log.part, log.num_parts);
    LOG_DBG("Log message: %s", log.msg.c_str());
    LOG_DBG("Log ID: %d", log.log_id);

    put_no_except(log, _log_event_q, 100ms);
}

py::tuple AresSerial::_decode_version(uint32_t version_num) {
    constexpr uint32_t mask = 0xFF;
    constexpr uint32_t minor_shift = 8;
    constexpr uint32_t major_shift = 16;

    uint32_t patch = version_num & mask;
    uint32_t minor = (version_num >> minor_shift) & mask;
    uint32_t major = (version_num >> major_shift) & mask;

    return py::make_tuple(major, minor, patch);
}

void AresSerial::_debug_event(const AresFrame::Dbg &msg) {
    if (msg.code != 0) {
        LOG_ERR("Received debug event: %d", msg.code);
    }
}

void AresSerial::_packet_rx_event(const AresFrame::PktRx &msg) {
    LOG_DBG("Received packet (%u, %u, %u)", msg.seq_cnt, msg.packet_id,
            msg.src_id);

    put_no_except(msg, _pkt_rx_event_q, 100ms);
}

void AresSerial::_packet_tx_event(const AresFrame::PktTx &msg) {
    LOG_DBG("Transmitted %u times", msg.count);
    put_no_except(msg, _pkt_tx_event_q, 100ms);
}

template <typename T, size_t size, bool overwrite>
static bool
put_noexcept(ares::bounded_queue<std::unique_ptr<T>, size, overwrite> &q) {
    bool exit_requested = true;

    try {
        q.put_nonblocking(static_cast<std::unique_ptr<T>>(nullptr));
    } catch (const ares::queue_exception &) {
        exit_requested = false;
    }

    return exit_requested;
}

bool AresSerial::_stop_event_queues() {
    bool success = put_noexcept(_start_event_q);
    success = put_noexcept(_poll_event_q) && success;
    success = put_noexcept(_log_event_q) && success;
    success = put_noexcept(_pkt_rx_event_q) && success;
    success = put_noexcept(_pkt_tx_event_q) && success;
    success = put_noexcept(_ble_connect_event_q) && success;
    success = put_noexcept(_ble_subscribe_event_q) && success;
    success = put_noexcept(_abort_event_q) && success;
    success = put_noexcept(_node_ready_event_q) && success;
    return success;
}

void AresSerial::_abort_event(const AresFrame::Abort &event) {
    LOG_INF("Abort event received (source id: %d, broadcast: %d)", event.id,
            event.broadcast);

    if (!event.broadcast) {
        _lora_ack_work.sem.take();
        _lora_ack_work.id = event.id;
        _lora_ack_work.acked_message = AresFrame::ABORT;
        _work_q.submit(&_lora_ack_work.work);
    }

    put_no_except(event, _abort_event_q, 100ms);
}

void AresSerial::_parse_node_config_kwargs(
    uint16_t id, std::vector<AresFrame::Frame> &frames,
    const py::kwargs &kwargs) {
    if (kwargs.contains("folder_dt") && !kwargs["folder_dt"].is_none()) {
        ares::DateTime dt(
            kwargs["folder_dt"].cast<std::chrono::system_clock::time_point>());
        AresFrame::NodeConfigSaveFolder save_folder{
            .year = static_cast<uint16_t>(dt.year()),
            .month = static_cast<uint8_t>(dt.month()),
            .day = static_cast<uint8_t>(dt.day()),
            .hour = static_cast<uint8_t>(dt.hour()),
            .minute = static_cast<uint8_t>(dt.minute()),
            .second = static_cast<uint8_t>(dt.second()),
        };
        AresFrame::Frame frame(AresFrame::NODE_CONFIG,
                               AresFrame::NodeConfig{
                                   id,
                                   AresFrame::SAVE_FOLDER,
                                   save_folder,
                               });
        frames.emplace_back(frame);
    }

    if (kwargs.contains("bandwidth") && !kwargs["bandwidth"].is_none()) {
        AresFrame::Frame frame(AresFrame::NODE_CONFIG,
                               AresFrame::NodeConfig{
                                   id,
                                   AresFrame::BANDWIDTH,
                                   kwargs["bandwidth"].cast<double>(),
                               });
        frames.emplace_back(frame);
    }

    if (kwargs.contains("center_freq") && !kwargs["center_freq"].is_none()) {
        AresFrame::Frame frame(AresFrame::NODE_CONFIG,
                               AresFrame::NodeConfig{
                                   id,
                                   AresFrame::CENTER_FREQ,
                                   kwargs["center_freq"].cast<double>(),
                               });
        frames.emplace_back(frame);
    }

    if (kwargs.contains("ref_level") && !kwargs["ref_level"].is_none()) {
        AresFrame::Frame frame(AresFrame::NODE_CONFIG,
                               AresFrame::NodeConfig{
                                   id,
                                   AresFrame::REF_LEVEL,
                                   kwargs["ref_level"].cast<double>(),
                               });
        frames.emplace_back(frame);
    }

    if (kwargs.contains("duration") && !kwargs["duration"].is_none()) {
        AresFrame::Frame frame(AresFrame::NODE_CONFIG,
                               AresFrame::NodeConfig{
                                   id,
                                   AresFrame::DURATION,
                                   kwargs["duration"].cast<uint32_t>(),
                               });
        frames.emplace_back(frame);
    }
}

py::dict
AresSerial::_send_node_config_frames(uint16_t id,
                                     std::vector<AresFrame::Frame> &frames,
                                     const std::chrono::seconds &ack_timeout) {
    py::dict ret;

    for (auto &frame : frames) {
        AresResponse response = _send_frame(frame, _response_timeout);

        int code = -1;

        switch (response.type) {
        case AresResponse::ACK: {
            code = std::get<AresFrame::Ack>(response.payload).code;
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

        switch (std::get<AresFrame::NodeConfig>(frame.tx_payload()).type) {
        case AresFrame::SAVE_FOLDER: {
            ret["folder_dt"] = code;
            break;
        }
        case AresFrame::BANDWIDTH: {
            ret["bandwidth"] = code;
            break;
        }
        case AresFrame::CENTER_FREQ: {
            ret["center_freq"] = code;
            break;
        }
        case AresFrame::DURATION: {
            ret["duration"] = code;
            break;
        }
        case AresFrame::REF_LEVEL: {
            ret["ref_level"] = code;
            break;
        }
        default: {
            throw std::runtime_error("Invalid node config sent");
        }
        }

        _wait_lora_ack(id, ack_timeout, AresFrame::NODE_CONFIG);
    }

    return ret;
}

void AresSerial::_handle_node_config_event(
    const AresFrame::NodeConfig &config) {
    LOG_INF("Received node config message from %d (type: %d)", config.id,
            static_cast<int>(config.type));
    _lora_ack_work.sem.take();
    _lora_ack_work.id = config.id;
    _lora_ack_work.acked_message = AresFrame::NODE_CONFIG;
    _work_q.submit(&_lora_ack_work.work);

    std::unique_lock lock(_node_configs.sem);

    switch (config.type) {
    case AresFrame::SAVE_FOLDER: {
        auto sf = std::get<AresFrame::NodeConfigSaveFolder>(config.config);
        LOG_DBG("Received folder name: (%d, %d, %d, %d, %d, %d)", sf.year,
                sf.month, sf.day, sf.hour, sf.minute, sf.second);
        _node_configs.save_folder = ares::DateTime(
            sf.year, sf.month, sf.day, sf.hour, sf.minute, sf.second);
        break;
    }
    case AresFrame::BANDWIDTH: {
        auto bw = std::get<double>(config.config);
        _node_configs.bandwidth = bw;
        break;
    }
    case AresFrame::CENTER_FREQ: {
        auto cf = std::get<double>(config.config);
        _node_configs.center_freq = cf;
        break;
    }
    case AresFrame::REF_LEVEL: {
        auto rl = std::get<double>(config.config);
        _node_configs.ref_level = rl;
        break;
    }
    case AresFrame::DURATION: {
        auto dur = std::get<uint32_t>(config.config);
        _node_configs.duration = dur;
        break;
    }
    default: {
        throw std::runtime_error("Received invalid config");
    }
    }
}

void AresSerial::_get_node_configs_released(NodeConfigs &copy) {
    py::gil_scoped_release release;
    std::unique_lock lock(_node_configs.sem);

    copy.save_folder = _node_configs.save_folder;
    copy.bandwidth = _node_configs.bandwidth;
    copy.center_freq = _node_configs.center_freq;
    copy.duration = _node_configs.duration;
    copy.ref_level = _node_configs.ref_level;
}

void AresSerial::_handle_node_config_poll_event(
    const AresFrame::NodeConfigPoll &event) {
    LOG_INF("Received node config poll request from %d (type: %d)", event.id,
            event.type);

    std::unique_lock lock(_node_configs.sem);

    _node_response_work.sem.take();
    _node_response_work.id = event.id;
    _node_response_work.type = event.type;

    switch (event.type) {
    case AresFrame::SAVE_FOLDER: {
        _node_response_work.config = AresFrame::NodeConfigSaveFolder{
            .year = static_cast<uint16_t>(_node_configs.save_folder.year()),
            .month = static_cast<uint8_t>(_node_configs.save_folder.month()),
            .day = static_cast<uint8_t>(_node_configs.save_folder.day()),
            .hour = static_cast<uint8_t>(_node_configs.save_folder.hour()),
            .minute = static_cast<uint8_t>(_node_configs.save_folder.minute()),
            .second = static_cast<uint8_t>(_node_configs.save_folder.second()),
        };
        break;
    }
    case AresFrame::BANDWIDTH: {
        _node_response_work.config = _node_configs.bandwidth;
        break;
    }
    case AresFrame::CENTER_FREQ: {
        _node_response_work.config = _node_configs.center_freq;
        break;
    }
    case AresFrame::DURATION: {
        _node_response_work.config = _node_configs.duration;
        break;
    }
    case AresFrame::REF_LEVEL: {
        _node_response_work.config = _node_configs.ref_level;
        break;
    }
    default: {
        throw std::runtime_error("Invalid config type polled for");
        break;
    }
    }

    LOG_DBG("Node response work submitted");
    _work_q.submit(&_node_response_work.work);
}

void AresSerial::_node_config_response_work_handler(ares::Work *work) {
    NodeConfigPollResponseWork *cwork =
        ares::container_of(work, &NodeConfigPollResponseWork::work);
    uint16_t id = cwork->id;
    AresFrame::NodeConfigType type = cwork->type;
    AresFrame::NodeConfigData config = cwork->config;
    cwork->sem.give();

    cwork->obj->_send_node_config_response(id, type, config);
}

void AresSerial::_send_node_config_response(uint16_t id,
                                            AresFrame::NodeConfigType type,
                                            AresFrame::NodeConfigData data) {
    AresFrame::Frame frame(AresFrame::NODE_CONFIG_RESP,
                           AresFrame::NodeConfigResponse{id, type, data});

    AresResponse response;

    try {
        response =
            _send_frame_released(frame, std::chrono::milliseconds::max());
    } catch (const std::exception &e) {
        LOG_ERR("_send_frame_released(): %s", e.what());
        return;
    }

    switch (response.type) {
    case AresResponse::ACK: {
        LOG_DBG("Send node config response ACK'ed: %d",
                std::get<AresFrame::Ack>(response.payload).code);
        break;
    }
    case AresResponse::BAD_FRAME: {
        LOG_ERR("Bad frame response received in node config response handler");
        break;
    }
    default: {
        LOG_ERR("Received invalid response");
        break;
    }
    }
}

void AresSerial::_handle_node_config_response_event(
    const AresFrame::NodeConfigResponse &event) {
    LOG_INF("Node config response received from %d (type %d)", event.id,
            event.type);

    {
        std::unique_lock lock(_node_config_response_sem);
        if (_expected_node_config_response_id != event.id) {
            LOG_WRN("Late config response received or spurious config response "
                    "received");
            return;
        }
    }

    put_no_except(event, _node_config_response_event_q, 100ms);
}

bool AresSerial::_wait_config_response(
    uint16_t id, const std::chrono::seconds &timeout,
    AresFrame::NodeConfigType expected_type,
    AresFrame::NodeConfigResponse &response) {
    py::gil_scoped_release release;
    bool timed_out = false;
    auto now = std::chrono::steady_clock::now;

    auto to_time = now() + timeout;
    while ((id != response.id || expected_type != response.type) &&
           !timed_out) {
        try {
            response = *_node_config_response_event_q.get(
                std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
        } catch (const ares::queue_exception &e) {
            timed_out = e.reason() == ares::queue_exception::QUEUE_TIMEOUT;
            continue;
        }

        timed_out = now() > to_time;
    }

    return timed_out;
}

void AresSerial::_parse_node_config_poll_args(
    uint16_t id, std::vector<AresFrame::Frame> &frames, const py::args &args) {
    struct {
        bool save_folder = false;
        bool bandwidth = false;
        bool center_freq = false;
        bool duration = false;
        bool ref_level = false;
    } parsed;

    for (const auto &arg : args) {
        if (!py::isinstance<py::str>(arg)) {
            // Not a string. Skip...
            continue;
        }

        const auto val = arg.cast<std::string>();

        if (!parsed.save_folder && val == "folder_dt") {
            parsed.save_folder = true;
            AresFrame::Frame frame(
                AresFrame::NODE_CONFIG_POLL,
                AresFrame::NodeConfigPoll{id, AresFrame::SAVE_FOLDER});
            frames.emplace_back(frame);
            continue;
        }

        if (!parsed.bandwidth && val == "bandwidth") {
            parsed.bandwidth = true;
            AresFrame::Frame frame(
                AresFrame::NODE_CONFIG_POLL,
                AresFrame::NodeConfigPoll{id, AresFrame::BANDWIDTH});
            frames.emplace_back(frame);
            continue;
        }

        if (!parsed.center_freq && val == "center_freq") {
            parsed.center_freq = true;
            AresFrame::Frame frame(
                AresFrame::NODE_CONFIG_POLL,
                AresFrame::NodeConfigPoll{id, AresFrame::CENTER_FREQ});
            frames.emplace_back(frame);
            continue;
        }

        if (!parsed.duration && val == "duration") {
            parsed.ref_level = true;
            AresFrame::Frame frame(
                AresFrame::NODE_CONFIG_POLL,
                AresFrame::NodeConfigPoll{id, AresFrame::DURATION});
            frames.emplace_back(frame);
            continue;
        }

        if (!parsed.ref_level && val == "ref_level") {
            parsed.ref_level = true;
            AresFrame::Frame frame(
                AresFrame::NODE_CONFIG_POLL,
                AresFrame::NodeConfigPoll{id, AresFrame::REF_LEVEL});
            frames.emplace_back(frame);
            continue;
        }
    }
}

py::dict AresSerial::_send_node_config_poll_frames(
    uint16_t id, std::vector<AresFrame::Frame> &frames,
    const std::chrono::seconds &ack_timeout) {
    py::dict ret;

    for (auto &frame : frames) {
        AresResponse response = _send_frame(frame, _response_timeout);

        int code = -1;

        switch (response.type) {
        case AresResponse::ACK: {
            code = std::get<AresFrame::Ack>(response.payload).code;
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

        auto expected_type =
            std::get<AresFrame::NodeConfigPoll>(frame.tx_payload()).type;
        AresFrame::NodeConfigResponse rx_response;
        bool timed_out =
            _wait_config_response(id, ack_timeout, expected_type, rx_response);

        switch (expected_type) {
        case AresFrame::SAVE_FOLDER: {
            if (timed_out) {
                ret["folder_dt"] = py::make_tuple(code, py::none());
            } else {
                auto rx_dt = std::get<AresFrame::NodeConfigSaveFolder>(
                    rx_response.config);
                ares::DateTime dt(rx_dt.year, rx_dt.month, rx_dt.day,
                                  rx_dt.hour, rx_dt.minute, rx_dt.second);
                ret["folder_dt"] = py::make_tuple(code, dt.time_point());
            }
            break;
        }
        case AresFrame::BANDWIDTH: {
            if (timed_out) {
                ret["bandwidth"] = py::make_tuple(code, py::none());
            } else {
                ret["bandwidth"] =
                    py::make_tuple(code, std::get<double>(rx_response.config));
            }
            break;
        }
        case AresFrame::CENTER_FREQ: {
            if (timed_out) {
                ret["center_freq"] = py::make_tuple(code, py::none());
            } else {
                ret["center_freq"] =
                    py::make_tuple(code, std::get<double>(rx_response.config));
            }
            break;
        }
        case AresFrame::DURATION: {
            if (timed_out) {
                ret["duration"] = py::make_tuple(code, py::none());
            } else {
                ret["duration"] = py::make_tuple(
                    code, std::get<uint32_t>(rx_response.config));
            }
            break;
        }
        case AresFrame::REF_LEVEL: {
            if (timed_out) {
                ret["ref_level"] = py::make_tuple(code, py::none());
            } else {
                ret["ref_level"] =
                    py::make_tuple(code, std::get<double>(rx_response.config));
            }
            break;
        }
        default: {
            throw std::runtime_error("Invalid node config sent");
        }
        }
    }

    return ret;
}

void AresSerial::_handle_node_ready_event(const AresFrame::NodeReady &event) {
    LOG_INF("Received node ready event from %d (broadcast: %d)", event.id,
            event.broadcast);

    if (!event.broadcast) {
        _lora_ack_work.sem.take();
        _lora_ack_work.id = event.id;
        _lora_ack_work.acked_message = AresFrame::NODE_READY;
        _work_q.submit(&_lora_ack_work.work);
    }

    put_no_except(event, _node_ready_event_q, 100ms);
}

void AresSerial::_ble_connect_event(const AresFrame::BleConnect &event) {
    LOG_DBG("Received BLE connect event (Connected: %d, MTU: %d)",
            event.connected, event.chunk_size);
    ble_info.connected = event.connected;
    ble_info.mtu = event.chunk_size;

    put_no_except(event, _ble_connect_event_q, 100ms);
}

void AresSerial::_ble_subscribe_event(const AresFrame::BleSubscribed &event) {
    LOG_DBG("Received BLE subscription event (chunk: %d, image: %d)",
            event.chunk, event.image);
    ble_info.subscriptions.chunk = event.chunk;
    ble_info.subscriptions.image = event.image;

    put_no_except(event, _ble_subscribe_event_q, 100ms);
}

int AresSerial::_ble_send_chunk(uint64_t num_chunks) {
    _check_crash();
    LOG_DBG("BLE chunk command received");
    AresFrame::Frame frame(AresFrame::BLE_CHUNK,
                           AresFrame::BleChunk{num_chunks});
    LOG_DBG("Image fits into %lu chunks", num_chunks);
    AresResponse response = _send_frame(frame, _response_timeout);

    int ret = -1;

    switch (response.type) {
    case AresResponse::ACK: {
        ret = std::get<AresFrame::Ack>(response.payload).code;
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

py::tuple AresSerial::_ble_send_image(const std::vector<uint8_t> &image) {
    _check_crash();
    LOG_DBG("BLE image command received");
    LOG_DBG_HEXDUMP(image, image.size(), "Bytes to send");
    AresFrame::Frame frame(AresFrame::BLE_IMAGE_CHUNK,
                           AresFrame::BleImage(image, ble_info.mtu));
    std::vector<AresResponse> responses;

    _send_multi_frame(frame, _response_timeout, responses);

    std::vector<int> ret;

    for (auto &response : responses) {
        switch (response.type) {
        case AresResponse::ACK: {
            ret.emplace_back(std::get<AresFrame::Ack>(response.payload).code);
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
    }

    return ares::array_to_tuple(ret.data(), ret.size());
}
