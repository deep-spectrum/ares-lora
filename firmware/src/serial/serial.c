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

#include "../../cmake-build-debug-nrf-connect-v3.2.2/zephyr/include/generated/zephyr/autoconf.h"

LOG_MODULE_REGISTER(ares_serial);

#define SERIAL_API_CALL(_serial, _api, ...)                                    \
    COND_CODE_1(IS_EMPTY(__VA_ARGS__),                                         \
                (_serial->iface->api->_api(_serial->iface)),                   \
                (_serial->iface->api->_api(_serial->iface, __VA_ARGS__)))

typedef void (*serial_signal_handler_t)(const struct ares_serial *serial);

static void ares_serial_process(const struct ares_serial *serial);

static void serial_signal_handle(const struct ares_serial *serial,
                                 enum ares_serial_signal signal,
                                 serial_signal_handler_t handler) {
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

static void transport_evt_handler(enum serial_transport_evt evt_type,
                                  void *ctx) {
    struct ares_serial *serial = ctx;
    struct k_poll_signal *signal;

    signal = (evt_type == SERIAL_TRANSPORT_EVT_RX_RDY)
                 ? &serial->ctx->signals[ARES_SIGNAL_RXRDY]
                 : &serial->ctx->signals[ARES_SIGNAL_TXDONE];
    k_poll_signal_raise(signal, 0);
}

static int instance_init(const struct ares_serial *serial,
                         const void *transport_config) {
    memset(serial->ctx, 0, sizeof(*serial->ctx));

    k_mutex_init(&serial->ctx->wr_mtx);

    for (size_t i = 0; i < ARES_SIGNALS; i++) {
        k_poll_signal_init(&serial->ctx->signals[i]);
        k_poll_event_init(&serial->ctx->events[i], K_POLL_TYPE_SIGNAL,
                          K_POLL_MODE_NOTIFY_ONLY, &serial->ctx->signals[i]);
    }

    return SERIAL_API_CALL(serial, init, transport_config,
                           transport_evt_handler, (void *)serial);
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

    tid = k_thread_create(serial->thread, serial->stack,
                          CONFIG_ARES_SERIAL_STACK_SIZE, serial_thread,
                          (void *)serial, NULL, NULL, CONFIG_ARES_SERIAL_PRIO,
                          K_ESSENTIAL, K_NO_WAIT);
    k_thread_name_set(tid, serial->name);
    serial->ctx->tid = tid;

    return 0;
}

int ares_serial_register_command_callbacks(
    const struct ares_serial *serial,
    const struct ares_serial_command *commands, size_t num_commands) {
    if (serial == NULL || serial->ctx == NULL ||
        (commands == NULL && num_commands != 0)) {
        return -EINVAL;
    }

    serial->ctx->commands = commands;
    serial->ctx->num_commands = num_commands;

    return 0;
}

static void report_error(const struct ares_serial *serial,
                         enum ares_frame_error error) {
    struct ares_frame frame = {
        .type = ARES_FRAME_FRAMING_ERROR,
        .payload.FRAMING_ERROR = error,
    };

    ares_serial_write_frame(serial, &frame);
}

static void dispatch(const struct ares_serial *serial, int start_index,
                     size_t length) {
    struct ares_frame frame;
    int ret;

    ret = ares_deserialize_frame(&frame, &serial->ctx->rx_buf.buf[start_index],
                                 length);
    if (ret < 0) {
        report_error(serial, ARES_FRAME_ERROR_BAD_FRAME);
        return;
    }

    for (size_t i = 0; i < serial->ctx->num_commands; i++) {
        if (serial->ctx->commands[i].command != frame.type) {
            continue;
        }

        if (serial->ctx->commands[i].callback != NULL) {
            serial->ctx->commands[i].callback(serial, &frame);
        } else {
            report_error(serial, ARES_FRAME_ERROR_NOT_IMPLEMENTED);
        }
        return;
    }

    report_error(serial, ARES_FRAME_ERROR_BAD_TYPE);
}

static void drop_data(const struct ares_serial *serial, struct ares_buf *buf) {
    size_t count;

    buf->len = 0;

    do {
        (void)SERIAL_API_CALL(serial, read, &buf->buf, sizeof(buf->buf),
                              &count);
    } while (count != 0);
}

static void ares_serial_process(const struct ares_serial *serial) {
    __ASSERT_NO_MSG(serial);
    __ASSERT_NO_MSG(serial->ctx);

    size_t count = 0;
    char data;
    struct ares_buf *buf = &serial->ctx->rx_buf;
    struct ares_frame_info info = {-1, -1, -1};

    while (true) {
        if (buf->len >= sizeof(buf->buf)) {
            LOG_ERR("Dropping data");
            drop_data(serial, buf);
            return;
        }

        (void)SERIAL_API_CALL(serial, read, &data, 1, &count);
        if (count == 0) {
            return;
        }
        LOG_DBG("Read byte: %d | 0x%X | '%c'", (int)data, (uint16_t)data,
                (char)data);

        buf->buf[buf->len] = data;
        buf->len++;

        if (data == ARES_FRAME_FOOTER || info.start_index >= 0) {
            int result;

            result = ares_serial_frame_present(buf->buf, buf->len, &info);
            __ASSERT_NO_MSG(result != -EINVAL);

            if (result == 0) {
                LOG_DBG(
                    "Frame info states: {start: %d, length: %d, remaining: %d}",
                    info.start_index, info.frame_size, info.bytes_left);
                continue;
            }

            LOG_DBG("Frame found. Frame length: %d", info.frame_size);
            dispatch(serial, info.start_index, info.frame_size);

            (void)memset(buf->buf, 0, buf->len);
            buf->len = 0;
            info.start_index = -1;
            info.frame_size = -1;
            info.bytes_left = -1;
        }
    }
}

static int ares_write(const struct ares_serial *serial, const void *data,
                      size_t nbytes) {
    __ASSERT_NO_MSG(serial);
    __ASSERT_NO_MSG(serial->ctx);
    __ASSERT_NO_MSG(data);
    __ASSERT_NO_MSG(nbytes);

    size_t offset = 0u, tmp_cnt, length = nbytes;

    (void)k_mutex_lock(&serial->ctx->wr_mtx, K_FOREVER);
    while (length != 0) {
        int err = SERIAL_API_CALL(
            serial, write, &((const uint8_t *)data)[offset], length, &tmp_cnt);
        ARG_UNUSED(err);

        __ASSERT_NO_MSG(err == 0);
        __ASSERT_NO_MSG(nbytes >= length);

        offset += tmp_cnt;
        length -= tmp_cnt;
    }
    (void)k_mutex_unlock(&serial->ctx->wr_mtx);

    return (int)nbytes;
}

int ares_serial_write_frame(const struct ares_serial *serial,
                            const struct ares_frame *frame) {
    uint8_t buffer[ARES_SERIAL_TRX_BUF_SIZE];
    int len;

    if (serial == NULL || frame == NULL) {
        return -EINVAL;
    }

    len = ares_serialize_frame(buffer, sizeof(buffer), frame);

    if (len < 0) {
        return len;
    }

    (void)ares_write(serial, buffer, len);
    return 0;
}

void ares_serial_flush_out(const struct ares_serial *serial,
                           k_timeout_t timeout) {
    unsigned int set;
    int res;
    struct k_poll_signal *sig;
    const uint8_t data[] = {'\0'};

    if (serial == NULL) {
        return;
    }

    sig = &serial->ctx->signals[ARES_SIGNAL_TXDONE];
    k_poll_signal_reset(sig);
    ares_write(serial, data, 1);

    k_poll(&serial->ctx->events[ARES_SIGNAL_TXDONE], 1, timeout);
    k_poll_signal_check(sig, &set, &res);
}

int wait_serial_ready(const struct ares_serial *serial) {
    if (serial == NULL) {
        return -EINVAL;
    }

    if (serial->iface->api->wait_dtr == NULL) {
        return -ENOTSUP;
    }
    SERIAL_API_CALL(serial, wait_dtr);
    return 0;
}

int set_wait_usb_host(const struct ares_serial *serial, bool block) {
    if (serial == NULL) {
        return -EINVAL;
    }

    SERIAL_API_CALL(serial, block_no_usb_host, block);
    return 0;
}

bool ares_serial_check_rx_error(const struct ares_serial *serial) {
    if (serial == NULL) {
        return true;
    }
    return SERIAL_API_CALL(serial, rx_error);
}
