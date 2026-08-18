/**
 * @file serial_driver.cpp
 *
 * @brief
 *
 * @date 8/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/serial-driver/frame_dispatcher.hpp>
#include <ares-lora-serial/serial-driver/serial_driver.hpp>
#include <ares/logging/log.hpp>
#include <ares/pyutil.hpp>

constexpr const char *folder_dt = "folder_dt";
constexpr const char *bw = "bandwidth";
constexpr const char *center_f = "center_freq";
constexpr const char *duration = "duration";
constexpr const char *ref_level = "ref_level";

LOG_MODULE_REGISTER(serial_logger);

void check_python_errors() {
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

py::tuple AresSerial::setting(const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::Setting payload;

    if (!kwargs.contains("id")) {
        // todo: throw kwargs error
    }

    payload.setting_id = kwargs["id"].cast<decltype(payload.setting_id)>();

    if (kwargs.contains("value")) {
        payload.value = kwargs["value"].cast<decltype(payload.value)>();
        payload.set = true;
    }

    FrameDispatcher dispatcher(*this, payload.set, kwargs);
    return dispatcher.send_frame(payload)
        .build_python_response<AresFrame::Setting>();
}

py::tuple AresSerial::lora_config(const AresLoraConfig &config,
                                  const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::LoraConfig payload(
        config.frequency, config.preamble_length, config.bandwidth,
        config.datarate, config.coding_rate, config.tx_power, 0, 0, 0, 0);

    FrameDispatcher dispatcher(*this, false, kwargs);
    return dispatcher.send_frame(payload).build_python_response();
}

py::tuple AresSerial::led(const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::Led payload;

    if (!kwargs.contains("id")) {
        // todo throw kwargs error
    }

    payload.led = kwargs["id"].cast<decltype(payload.led)>();

    if (kwargs.contains("state")) {
        payload.state = static_cast<decltype(payload.state)>(
            kwargs["state"].cast<uint8_t>());
    } else {
        payload.state = AresFrame::Led::FETCH;
    }

    FrameDispatcher dispatcher(*this, true, kwargs);
    return dispatcher.send_frame(payload)
        .build_python_response<AresFrame::Led>();
}

py::tuple AresSerial::version(const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::Version payload;

    FrameDispatcher dispatcher(*this, true, kwargs);
    return dispatcher.send_frame(payload)
        .build_python_response<AresFrame::Version>();
}

py::tuple AresSerial::reboot(const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::Reboot payload;

    if (kwargs.contains("delay")) {
        payload.delay = kwargs["delay"].cast<decltype(payload.delay)>();
        if (payload.delay < 5) {
            payload.delay = 5;
        } else if (payload.delay > 30) {
            payload.delay = 30;
        }
    }

    FrameDispatcher dispatcher(*this, false, kwargs);
    py::tuple ret = dispatcher.send_frame(payload).build_python_response();
    stop_driver();
    _serial.close();

    {
        py::gil_scoped_release release;
        std::this_thread::sleep_for(std::chrono::seconds(payload.delay + 1));
    }

    return ret;
}

template <typename Payload, typename AckType = AresFrame::LoraAck>
static py::tuple send_broadcastable_lora_msg(
    FrameDispatcher &dispatcher, Payload &payload,
    const std::string &msg = "Timed out waiting for ACK") {
    if (dispatcher.broadcast_msg()) {
        return dispatcher.send_frame<Payload>(payload)
            .template build_python_response<AckType>();
    }
    return dispatcher.send_frame<Payload>(payload)
        .template build_python_response<AckType, AresTimeoutError>(msg);
}

py::tuple AresSerial::start(int64_t second, uint64_t usec,
                            const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::Start payload;

    payload.sec = second;
    payload.usec = usec;

    FrameDispatcher dispatcher(*this, false, kwargs);
    return send_broadcastable_lora_msg(dispatcher, payload);
}

py::tuple AresSerial::poll(const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::Poll payload;

    FrameDispatcher dispatcher(*this, false, kwargs);
    return dispatcher.send_frame(payload)
        .build_python_response<AresFrame::Heartbeat, AresTimeoutError>(
            "Timed out waiting for heartbeat");
}

py::tuple AresSerial::log(const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::Log payload;
    payload.log_id = _log_id;
    _log_id++;

    if (kwargs.contains("message")) {
        payload.msg = kwargs["message"].cast<std::string>();
    }

    FrameDispatcher dispatcher(*this, false, kwargs);
    return send_broadcastable_lora_msg<decltype(payload), AresFrame::LogAck>(
        dispatcher, payload);
}

py::tuple AresSerial::abort(const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::Abort payload;

    FrameDispatcher dispatcher(*this, false, kwargs);
    return send_broadcastable_lora_msg(dispatcher, payload);
}

static void _parse_node_config_kwargs(
    std::map<std::string, AresFrame::NodeConfig> &payloads,
    const py::kwargs &kwargs) {
    if (kwargs.contains("folder_dt")) {
        ares::DateTime dt(
            kwargs["folder_dt"].cast<std::chrono::system_clock::time_point>());
        payloads["folder_dt"] =
            AresFrame::NodeConfig(0, AresFrame::SAVE_FOLDER, dt);
    }

    if (kwargs.contains("bandwidth")) {
        payloads["bandwidth"] = AresFrame::NodeConfig(
            0, AresFrame::BANDWIDTH, kwargs["bandwidth"].cast<double>());
    }

    if (kwargs.contains("center_freq")) {
        payloads["center_freq"] = AresFrame::NodeConfig(
            0, AresFrame::CENTER_FREQ, kwargs["center_freq"].cast<double>());
    }

    if (kwargs.contains("ref_level")) {
        payloads["ref_level"] = AresFrame::NodeConfig(
            0, AresFrame::REF_LEVEL, kwargs["ref_level"].cast<double>());
    }

    if (kwargs.contains("duration")) {
        payloads["duration"] = AresFrame::NodeConfig(
            0, AresFrame::DURATION, kwargs["duration"].cast<uint32_t>());
    }
}

py::dict AresSerial::node_config(const py::kwargs &kwargs) {
    _check_crash();

    std::map<std::string, AresFrame::NodeConfig> payloads;
    _parse_node_config_kwargs(payloads, kwargs);

    FrameDispatcher dispatcher(*this, false, kwargs);
    py::dict ret;
    for (auto &[key, payload] : payloads) {
        ret[key.c_str()] = dispatcher.send_frame(payload)
                               .build_python_response<AresFrame::LoraAck>();
    }
    return ret;
}

static void _parse_node_config_args(
    std::map<std::string, AresFrame::NodeConfigPoll> &payloads,
    const py::args &args) {
    for (const auto &arg : args) {
        if (!py::isinstance<py::str>(arg)) {
            // Not a string. Skip....
            continue;
        }

        const auto val = arg.cast<std::string>();
        LOG_DBG("Poll parser parsed \"%s\"", val.c_str());

        if (val == "folder_dt") {
            payloads[val] =
                AresFrame::NodeConfigPoll(0, AresFrame::SAVE_FOLDER);
            continue;
        }

        if (val == "bandwidth") {
            payloads[val] = AresFrame::NodeConfigPoll(0, AresFrame::BANDWIDTH);
            continue;
        }

        if (val == "center_freq") {
            payloads[val] =
                AresFrame::NodeConfigPoll(0, AresFrame::CENTER_FREQ);
            continue;
        }

        if (val == "duration") {
            payloads[val] = AresFrame::NodeConfigPoll(0, AresFrame::DURATION);
            continue;
        }

        if (val == "ref_level") {
            payloads[val] = AresFrame::NodeConfigPoll(0, AresFrame::REF_LEVEL);
        }
    }
}

py::dict AresSerial::node_config_poll(const py::args &args,
                                      const py::kwargs &kwargs) {
    _check_crash();

    std::map<std::string, AresFrame::NodeConfigPoll> payloads;
    _parse_node_config_args(payloads, args);

    FrameDispatcher dispatcher(*this, false, kwargs);
    py::dict ret;
    for (auto &[key, payload] : payloads) {
        ret[key.c_str()] =
            dispatcher.send_frame(payload)
                .build_python_response<AresFrame::NodeConfigResponse>();
    }
    return ret;
}

py::tuple AresSerial::notify_run_ready(const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::NodeReady payload;

    FrameDispatcher dispatcher(*this, false, kwargs);
    return send_broadcastable_lora_msg(dispatcher, payload);
}

py::tuple AresSerial::ble_state(const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::BleState payload(AresFrame::BleState::REQUEST);

    if (kwargs.contains("state")) {
        payload.state = static_cast<AresFrame::BleState::State>(
            kwargs["state"].cast<uint8_t>());
    }

    FrameDispatcher dispatcher(*this, true, kwargs);
    return dispatcher.send_frame(payload)
        .build_python_response<AresFrame::BleState>();
}

py::tuple AresSerial::ble_disconnect(const py::kwargs &kwargs) {
    _check_crash();
    AresFrame::BleDisconnect payload;
    FrameDispatcher dispatcher(*this, false, kwargs);
    return dispatcher.send_frame(payload).build_python_response();
}

void AresSerial::set_ready(bool new_state) {
    py::gil_scoped_release release;
    std::unique_lock lock(_ready_mtx);
    _ready = new_state;
}

bool AresSerial::get_ready() {
    bool ret;
    py::gil_scoped_release release;
    std::unique_lock lock(_ready_mtx);
    ret = _ready;
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
        break;
    }
    }
}

long AresSerial::get_log_level() {
    _check_crash();
    return LOG_MODULE_CURRENT_LEVEL;
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

uint32_t AresSerial::wait_packet_tx_event() {
    AresFrame::PktTx event;
    wait_event_queue_released(event, _pkt_tx_event_q);
    return event.count;
}

bool AresSerial::wait_ble_connection_event() {
    AresFrame::BleConnect event;
    wait_event_queue_released(event, _ble_connect_event_q);
    return event.connected;
}

py::tuple AresSerial::wait_ble_subscribe_event() {
    AresFrame::BleSubscribed event;
    wait_event_queue_released(event, _ble_subscribed_event_q);
    return py::make_tuple(event.chunk, event.image);
}

py::tuple AresSerial::wait_abortion_event() {
    AresFrame::Abort event;
    wait_event_queue_released(event, _abortion_event_q);
    return py::make_tuple(event.id, event.broadcast);
}

py::tuple AresSerial::wait_run_ready_event() {
    AresFrame::NodeReady event;
    wait_event_queue_released(event, _run_ready_event_q);
    return py::make_tuple(event.id, event.broadcast);
}

void AresSerial::start_driver() {
    if (_tasks_running) {
        throw std::runtime_error("Already running");
    }

    LOG_INF("Starting driver");

    LOG_DBG("Clearing event queues");
    _start_event_q.clear();
    _log_event_q.clear();
    _pkt_rx_event_q.clear();
    _pkt_tx_event_q.clear();
    _ble_connect_event_q.clear();
    _ble_subscribed_event_q.clear();
    _abortion_event_q.clear();
    _run_ready_event_q.clear();

    LOG_DBG("Clearing message queues");
    _frame_q.clear();
    _response_queue.clear();
    _lora_response_q.clear();

    _exception = nullptr;
    if (_serial.is_closed()) {
        LOG_DBG("Port was closed. Attempting to open it.");
        _serial.open();
    }

    LOG_DBG("Starting LoRa Response Task");
    _tasks_running = true;
    _lora_response_task.set_essential(true);
    _lora_response_task.start();

    LOG_DBG("Starting processing task");
    _processing_task.set_essential(true);
    _processing_task.start();

    LOG_DBG("Starting RX Task");
    _rx_task.set_essential(true);
    _rx_task.start();
}

template <typename Signature, typename QueueType, size_t size, bool overwrite>
static bool
stop_driver_task(ares::Task<Signature> &task,
                 ares::bounded_queue<QueueType, size, overwrite> &queue,
                 const QueueType &terminate_val, size_t max_attempts) {
    size_t retries = 0;

    queue.clear();
    do {
        queue.put(terminate_val);
        if (task.join(100ms) == 0) {
            break;
        }
        retries++;
    } while (retries < max_attempts);

    return retries >= max_attempts;
}

void AresSerial::stop_driver() {
    constexpr size_t max_attempts = 10;
    if (!_tasks_running && !_exception) {
        return;
    }

    LOG_INF("Stopping driver");
    _tasks_running = false;
    _rx_task.join();

    AresFrame::Frame terminate_request{std::monostate()};
    bool failed_proc_task_shutdown = stop_driver_task(
        _processing_task, _frame_q, terminate_request, max_attempts);
    bool failed_lora_resp_task_shutdown =
        stop_driver_task(_lora_response_task, _lora_response_q,
                         terminate_request.tx_payload(), max_attempts);

    if (failed_proc_task_shutdown || failed_lora_resp_task_shutdown) {
        std::stringstream ss;
        if (failed_proc_task_shutdown) {
            ss << "Failed to shut down processing task";
        }
        if (failed_proc_task_shutdown && failed_lora_resp_task_shutdown) {
            ss << " and ";
        }
        if (failed_lora_resp_task_shutdown) {
            ss << "Failed to shut down lora response task";
        }
        throw std::runtime_error(ss.str());
    }

    _response_queue.clear();
    _lora_response_q.clear();
    _frame_q.clear();
}

py::dict AresSerial::get_node_config() {
    NodeConfigs copy;
    {
        py::gil_scoped_release release;
        std::unique_lock lock(_node_configs.sem);
        copy.bandwidth = _node_configs.bandwidth;
        copy.center_freq = _node_configs.center_freq;
        copy.duration = _node_configs.duration;
        copy.ref_level = _node_configs.ref_level;
        copy.save_folder = _node_configs.save_folder;
    }

    py::dict ret;
    ret[folder_dt] = copy.save_folder.time_point();
    ret[bw] = copy.bandwidth;
    ret[center_f] = copy.center_freq;
    ret[duration] = copy.duration;
    ret[ref_level] = copy.ref_level;

    return ret;
}

void AresSerial::cancel_events() {
    LOG_DBG("cancelling event queues");
    _stop_event_queues();
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
    EventDispatcher dispatcher{*this};

    while (_tasks_running || !stopped) {
        AresFrame::Frame frame = _frame_q.get();
        LOG_DBG("Received frame %d", frame.type());

        if (frame.type() == AresFrame::DRIVER_STOP) {
            stopped = _stop_event_queues();
            continue;
        }

        std::visit(dispatcher, frame.rx_payload());
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

void AresSerial::_lora_responses_check_fw_responses(
    const std::vector<FrameResponse> &responses,
    const AresFrame::AresFrameType &sent_type) {

    for (const auto &response : responses) {
        switch (response.type) {
        case FrameResponse::ACK: {
            LOG_DBG("Firmware responded to frame %d with ACK code %d",
                    sent_type, std::get<AresFrame::Ack>(response.payload).code);
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
        auto payload = _lora_response_q.get();

        if (std::holds_alternative<std::monostate>(payload)) {
            continue;
        }

        FrameDispatcher dispatcher(*this, _send_lora_response_timeout);

        try {
            dispatcher.send_frame_released(payload, responses);
        } catch (const std::exception &exc) {
            LOG_ERR("_send_frame_released(): %s", exc.what());
        }

        _lora_responses_check_fw_responses(responses,
                                           dispatcher.type_dispatched());
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

template <typename T, size_t size, bool overwrite>
static bool queue_nullptr(
    ares::bounded_queue<std::unique_ptr<T>, size, overwrite> &q) noexcept {
    bool exit_requested = true;

    try {
        q.put_nonblocking(static_cast<std::unique_ptr<T>>(nullptr));
        LOG_DBG("Exit successfully requested");
    } catch (const ares::queue_exception &exc) {
        exit_requested = false;
        LOG_DBG("Exit not requested successfully: %s", exc.what());
    }

    return exit_requested;
}

bool AresSerial::_stop_event_queues() {
    LOG_DBG("Stopping event queues");
    bool success = queue_nullptr(_start_event_q);
    success = queue_nullptr(_log_event_q) && success;
    success = queue_nullptr(_pkt_rx_event_q) && success;
    success = queue_nullptr(_pkt_tx_event_q) && success;
    success = queue_nullptr(_ble_connect_event_q) && success;
    success = queue_nullptr(_ble_subscribed_event_q) && success;
    success = queue_nullptr(_abortion_event_q) && success;
    success = queue_nullptr(_run_ready_event_q) && success;
    LOG_DBG("Stop event queues yielded %d", success);
    return success;
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::Start &event) const {
    LOG_INF("Start event received: (%ld, %lu, %u, %d, %d, %u)", event.sec,
            event.usec, event.id, event.broadcast, event.seq_cnt,
            event.packet_id);

    if (!event.broadcast) {
        self._lora_response_q.put(
            AresFrame::LoraAck{event.id, AresFrame::START});
    }

    put_no_except(event, self._start_event_q, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::Heartbeat &event) const {
    LOG_INF("Heartbeat received from %d", event.id);
    AresFrame::Frame::AckTypes ack_event = event;
    put_no_except(ack_event, self._ack_queue, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::Poll &event) const {
    LOG_INF("Poll event received");
    std::unique_lock lock(self._ready_mtx);
    bool ready = self._ready;
    lock.unlock();
    self._lora_response_q.put(AresFrame::Heartbeat{ready, event.id});
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::Log &event) const {
    LOG_INF("Log event received from %d", event.id);
    LOG_DBG("Part %d of %d", event.part, event.num_parts);
    LOG_DBG("Log message: %s", event.msg.c_str());
    LOG_DBG("Log ID: %d", event.log_id);

    if (!event.broadcast) {
        self._lora_response_q.put(AresFrame::LogAck{event.part, event.num_parts,
                                                    event.log_id, event.id});
    }

    put_no_except(event, self._log_event_q, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::LogAck &event) const {
    LOG_INF("Log ACK event received (part %d of %d, %d)", event.part,
            event.num_parts, event.id);
    AresFrame::Frame::AckTypes ack_event = event;
    put_no_except(ack_event, self._ack_queue, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::Dbg &event) const {
    if (event.code != 0) {
        LOG_ERR("Received debug event: %d", event.code);
    }
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::PktRx &event) const {
    LOG_DBG("Received packet (%u, %u, %u)", event.seq_cnt, event.packet_id,
            event.src_id);
    put_no_except(event, self._pkt_rx_event_q, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::PktTx &event) const {
    LOG_DBG("Transmitted %u times", event.count);
    put_no_except(event, self._pkt_tx_event_q, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::BleConnect &event) const {
    LOG_DBG("Received BLE connect event (Connected: %d, MTU: %d)",
            event.connected, event.chunk_size);
    self._ble_info.connected = event.connected;
    self._ble_info.mtu_size = event.chunk_size;
    put_no_except(event, self._ble_connect_event_q, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::BleSubscribed &event) const {
    LOG_DBG("Received BLE subscription event (chunk: %d, image: %d)",
            event.chunk, event.image);
    self._ble_info.subscriptions.chunk = event.chunk;
    self._ble_info.subscriptions.image = event.image;
    put_no_except(event, self._ble_subscribed_event_q, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::Abort &event) const {
    LOG_INF("Abort event received (source id: %d, broadcast: %d)", event.id,
            event.broadcast);

    if (!event.broadcast) {
        self._lora_response_q.put(
            AresFrame::LoraAck{event.id, AresFrame::ABORT});
    }

    put_no_except(event, self._abortion_event_q, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::NodeConfig &event) const {
    LOG_INF("Received node config message from %d (type: %d)", event.id,
            static_cast<int>(event.type));

    self._lora_response_q.put(
        AresFrame::LoraAck{event.id, AresFrame::NODE_CONFIG});

    std::unique_lock lock(self._node_configs.sem);
    switch (event.type) {
    case AresFrame::SAVE_FOLDER: {
        self._node_configs.save_folder = std::get<ares::DateTime>(event.config);
        break;
    }
    case AresFrame::BANDWIDTH: {
        self._node_configs.bandwidth = std::get<double>(event.config);
        ;
        break;
    }
    case AresFrame::CENTER_FREQ: {
        self._node_configs.center_freq = std::get<double>(event.config);
        break;
    }
    case AresFrame::REF_LEVEL: {
        self._node_configs.ref_level = std::get<double>(event.config);
        break;
    }
    case AresFrame::DURATION: {
        self._node_configs.duration = std::get<uint32_t>(event.config);
        break;
    }
    default: {
        LOG_ERR("Received invalid node configuration");
        break;
    }
    }
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::NodeConfigPoll &event) const {
    LOG_INF("Received node config poll request from %d (type: %d)", event.id,
            event.type);

    AresFrame::NodeConfigData config;
    std::unique_lock lock(self._node_configs.sem);

    switch (event.type) {
    case AresFrame::SAVE_FOLDER: {
        config = self._node_configs.save_folder;
        break;
    }
    case AresFrame::BANDWIDTH: {
        config = self._node_configs.bandwidth;
        break;
    }
    case AresFrame::CENTER_FREQ: {
        config = self._node_configs.center_freq;
        break;
    }
    case AresFrame::REF_LEVEL: {
        config = self._node_configs.ref_level;
        break;
    }
    case AresFrame::DURATION: {
        config = self._node_configs.duration;
        break;
    }
    default: {
        LOG_ERR("Invalid configuration polled for");
        return;
    }
    }

    lock.unlock();

    self._lora_response_q.put(
        AresFrame::NodeConfigResponse{event.id, event.type, config});
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::NodeConfigResponse &event) const {
    LOG_INF("Node config response received from %d (type %d)", event.id,
            event.type);
    AresFrame::Frame::AckTypes ack_event = event;
    put_no_except(ack_event, self._ack_queue, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::NodeReady &event) const {
    LOG_INF("Received node ready event from %d (broadcast: %d)", event.id,
            event.broadcast);

    if (!event.broadcast) {
        self._lora_response_q.put(
            AresFrame::LoraAck{event.id, AresFrame::NODE_READY});
    }

    put_no_except(event, self._run_ready_event_q, 100ms);
}

void AresSerial::EventDispatcher::operator()(
    const AresFrame::LoraAck &event) const {
    LOG_INF("Received lora acknowledgement message (message_id: %u, message "
            "acked: %u)",
            event.id, event.message_type);
    AresFrame::Frame::AckTypes ack_event = event;
    put_no_except(ack_event, self._ack_queue, 100ms);
}
