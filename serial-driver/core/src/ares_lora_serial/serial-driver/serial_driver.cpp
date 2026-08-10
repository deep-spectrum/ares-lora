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
#include <ares/logging/log.hpp>
#include <ares/pyutil.hpp>

LOG_MODULE_REGISTER(serial_logger);

static void check_python_errors() {
    py::gil_scoped_acquire acquire;

    if (PyErr_CheckSignals() != 0) {
        throw py::error_already_set();
    }
}

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
      _processing_task([this] { _process_frames(); }),
      _lora_response_task([this] { _send_lora_responses(); }) {
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

void AresSerial::set_ready(bool new_state) { _ready = new_state; }

bool AresSerial::get_ready() const { return _ready; }

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
        break;
    }
    }
}

long AresSerial::get_log_level() {
    _check_crash();
    return LOG_MODULE_CURRENT_LEVEL;
}

void AresSerial::_check_crash() {
    if (_exception) {
        stop_driver();
        std::rethrow_exception(_exception);
    }
}

void AresSerial::_process_rx_buffer(std::vector<uint8_t> &buf) {
    while (true) {
        LOG_DBG("Processing %u bytes", buf.size());
        auto [frame_start, frame_size, _] =
            AresFrame::Frame::frame_present(buf);
        if (frame_start < 0) {
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
        } catch (const std::exception &exc) {
            _exception = std::current_exception();
            _serial.close();
            _tasks_running = false;
            LOG_ERR("RX task crashed. Stopping driver. Reason: %s", exc.what());
        }
    }
}

void AresSerial::_process_frames_helper() {
    bool stopped = false;
    while (_tasks_running || !stopped) {
        AresFrame::Frame frame = _frame_q.get();
        LOG_DBG("Received frame %d", frame.type());

        // todo: Do something with the frame.
    }
}

void AresSerial::_process_frames() {
    LOG_DBG("Starting processing task");
    while (_tasks_running) {
        try {
            _process_frames_helper();
        } catch (const std::exception &exc) {
            _exception = std::current_exception();
            _tasks_running = false;
            LOG_ERR("Processing task crashed. Stopping driver. Reason: %s",
                    exc.what());
        }
    }
}

void AresSerial::_send_frame(AresFrame::Frame &frame,
                             const std::chrono::milliseconds &timeout,
                             std::vector<FrameResponse> &responses) {
    py::gil_scoped_release release;
    _send_frame_released(frame, timeout, responses);
}

void AresSerial::_send_frame_released(AresFrame::Frame &frame,
                                      const std::chrono::milliseconds &timeout,
                                      std::vector<FrameResponse> &responses) {
    std::unique_lock lock(_command_lock, std::defer_lock);
    std::vector<uint8_t> buf;

    do {
        lock.lock();
        frame.serialize(buf);
        _response_queue.clear();
        _send_frame_released(buf);
        responses.emplace_back(_wait_response(timeout));
        lock.unlock();
        check_python_errors();
    } while (frame.frame_available());
}

void AresSerial::_send_frame_released(const std::vector<uint8_t> &buf) {
    LOG_DBG_HEXDUMP(buf, buf.size(), "Sending frame");
    std::unique_lock lock(_serial_lock);
    _serial.write(buf);
}

AresSerial::FrameResponse
AresSerial::_wait_response(const std::chrono::milliseconds &timeout) {
    if (timeout == ares::forever) {
        return _wait_response_forever();
    }

    return _wait_response_timeout(timeout);
}

AresSerial::FrameResponse
AresSerial::_wait_response_timeout(const std::chrono::milliseconds &timeout) {
    FrameResponse response;

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

AresSerial::FrameResponse AresSerial::_wait_response_forever() {
    // TODO: This will block Python exceptions. Add a lambda to queue.get to
    // check some condition.
    return _response_queue.get();
}

void AresSerial::_lora_responses_check_fw_responses(
    const std::vector<FrameResponse> &responses,
    const AresFrame::Frame &sent_frame) {

    for (const auto &response : responses) {
        switch (response.type) {
        case FrameResponse::ACK: {
            LOG_DBG("Firmware responded to frame %d with ACK code %d",
                    sent_frame.type(),
                    std::get<AresFrame::Ack>(response.payload).code);
            break;
        }
        case FrameResponse::BAD_FRAME: {
            LOG_ERR("Bad frame response received in a LoRa response");
            break;
        }
        default: {
            LOG_ERR("LoRa response messages must respond back with an ACK "
                    "message or a bad frame message");
            break;
        }
        }
    }
}

void AresSerial::_send_lora_responses_helper() {
    while (_tasks_running) {
        std::vector<FrameResponse> responses;
        auto frame = _lora_response_q.get();

        try {
            _send_frame_released(frame, _send_lora_response_timeout, responses);
        } catch (const std::exception &exc) {
            LOG_ERR("_send_frame_released(): %s", exc.what());
        }

        _lora_responses_check_fw_responses(responses, frame);
    }
}

void AresSerial::_send_lora_responses() {
    LOG_DBG("Starting response task");
    while (_tasks_running) {
        try {
            _send_lora_responses_helper();
        } catch (const std::exception &exc) {
            _exception = std::current_exception();
            _tasks_running = false;
            LOG_ERR(
                "Send LoRa response task crashed. Stopping driver. Reason: %s",
                exc.what());
        }
    }
}
