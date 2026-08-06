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
