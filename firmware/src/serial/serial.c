/**
 * @file serial.c
 *
 * @brief
 *
 * @date 3/20/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <serial/serial.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ares_serial);

#define SERIAL_API_CALL(_serial, _api, ...) COND_CODE_1(IS_EMPTY(__VA_ARGS__), (_serial->iface->api->_api(_serial->iface)), (_serial->iface->api->_api(_serial->iface, __VA_ARGS__)))

typedef void (*serial_signal_handler_t)(const struct ares_serial *serial);

void ares_serial_process(const struct ares_serial *serial);

static void serial_signal_handle(const struct ares_serial *serial, enum ares_serial_signal signal, serial_signal_handler_t handler) {
    struct k_poll_signal *sig = &serial->ctx->signals[signal];
    int res;
    unsigned int set;

    k_poll_signal_check(sig, &set, &res);

    if (set) {
        k_poll_signal_reset(sig);
        handler(serial);
    }
}

static void serial_thread(void *serial_handle, void *p2, void *p3) {
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct ares_serial *serial = serial_handle;
    int err;

    err = SERIAL_API_CALL(serial, enable, false);

    if (err != 0) {
        k_thread_abort(k_current_get());
    }

    while (true) {
        err = k_poll(serial->ctx->events, ARES_SIGNAL_TXDONE, K_FOREVER);

        if (err != 0) {
            k_mutex_lock(&serial->ctx->wr_mtx, K_FOREVER);
            LOG_ERR("Serial thread error: %d", err);
            k_mutex_unlock(&serial->ctx->wr_mtx);
            k_thread_abort(k_current_get());
        }

        serial_signal_handle(serial, ARES_SIGNAL_RXRDY, ares_serial_process);
    }

    __ASSERT(false, "serial_thread terminating");
}

static void transport_evt_handler(enum serial_transport_evt evt_type, void *ctx) {
    struct ares_serial *serial = ctx;
    struct k_poll_signal *signal;

    signal = (evt_type == SERIAL_TRANSPORT_EVT_RX_RDY) ? &serial->ctx->signals[ARES_SIGNAL_RXRDY] : &serial->ctx->signals[ARES_SIGNAL_TXDONE];
    k_poll_signal_raise(signal, 0);
}

static int instance_init(const struct ares_serial *serial, const void *transport_config) {
    memset(serial->ctx, 0, sizeof(*serial->ctx));

    k_mutex_init(&serial->ctx->wr_mtx);

    for (size_t i = 0; i < ARES_SIGNALS; i++) {
        k_poll_signal_init(&serial->ctx->signals[i]);
        k_poll_event_init(&serial->ctx->events[i], K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &serial->ctx->signals[i]);
    }

    return SERIAL_API_CALL(serial, init, transport_config, transport_evt_handler, (void *)serial);
}

int ares_serial_init(const struct ares_serial *serial,
                     const void *transport_config) {
    __ASSERT_NO_MSG(serial);
    __ASSERT_NO_MSG(transport_config);
    int err;
    k_tid_t tid;

    if (serial->ctx->tid) {
        return -EALREADY;
    }

    err = instance_init(serial, transport_config);

    if (err < 0) {
        return err;
    }

    tid = k_thread_create(serial->thread, serial->stack, CONFIG_ARES_SERIAL_STACK_SIZE, serial_thread, (void *)serial, NULL, NULL, CONFIG_ARES_SERIAL_PRIO, K_ESSENTIAL, K_NO_WAIT);
    k_thread_name_set(tid, serial->name);
    serial->ctx->tid = tid;

    return 0;
}

int ares_serial_register_command_callbacks(const struct ares_serial *serial, const struct ares_serial_command *commands, size_t num_commands) {
    if (serial == NULL || serial->ctx == NULL || (commands == NULL && num_commands != 0)) {
        return -EINVAL;
    }

    serial->ctx->commands = commands;
    serial->ctx->num_commands = num_commands;

    return 0;
}
