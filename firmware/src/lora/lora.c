/**
 * @file lora.c
 *
 * @brief
 *
 * @date 3/24/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <lora/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ares_lora);

#define LORA_API_CALL(_lora, _api, ...)                                        \
    COND_CODE_1(IS_EMPTY(__VA_ARGS__),                                         \
                (_lora->iface->api->_api(_lora->iface)),                       \
                (_lora->iface->api->_api(_lora->iface, __VA_ARGS__)))

typedef void(lora_signal_handler_t)(const struct ares_lora *lora);

static void ares_lora_process(const struct ares_lora *lora);

static void lora_signal_handle(const struct ares_lora *lora,
                               enum ares_lora_signal signal,
                               lora_signal_handler_t handler) {
    struct k_poll_signal *sig = &lora->ctx->signals[signal];
    int res;
    unsigned int set;

    k_poll_signal_check(sig, &set, &res);

    if (set) {
        k_poll_signal_reset(sig);
        handler(lora);
    }
}

static void lora_thread(void *lora_handle, void *p2, void *p3) {
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct ares_lora *lora = lora_handle;
    int err;

    err = LORA_API_CALL(lora, enable, false);

    if (err != 0) {
        k_thread_abort(k_current_get());
    }

    while (true) {
        err = k_poll(lora->ctx->events, ARES_LORA_SIGNAL_TXDONE, K_FOREVER);

        if (err != 0) {
            LOG_ERR("Lora thread error: %d", err);
            k_thread_abort(k_current_get());
        }

        lora_signal_handle(lora, ARES_LORA_SIGNAL_RXRDY, ares_lora_process);
    }

    __ASSERT(false, "Lora thread terminating");
}

static void transport_evt_handler(enum lora_transport_evt evt_type, void *ctx) {
    struct ares_lora *lora = ctx;
    struct k_poll_signal *signal;

    signal = (evt_type == LORA_TRANSPORT_EVT_RX_RDY)
                 ? &lora->ctx->signals[ARES_LORA_SIGNAL_RXRDY]
                 : &lora->ctx->signals[ARES_LORA_SIGNAL_TXDONE];
    k_poll_signal_raise(signal, 0);
}

static int instance_init(const struct ares_lora *lora,
                         const void *transport_config) {
    memset(lora->ctx, 0, sizeof(*lora->ctx));

    k_mutex_init(&lora->ctx->wr_mtx);

    for (size_t i = 0; i < ARES_LORA_SIGNALS; i++) {
        k_poll_signal_init(&lora->ctx->signals[i]);
        k_poll_event_init(&lora->ctx->events[i], K_POLL_TYPE_SIGNAL,
                          K_POLL_MODE_NOTIFY_ONLY, &lora->ctx->signals[i]);
    }

    return LORA_API_CALL(lora, init, transport_config, transport_evt_handler,
                         (void *)lora);
}

int ares_lora_init(const struct ares_lora *lora, const void *transport_config) {
    __ASSERT_NO_MSG(lora);
    __ASSERT_NO_MSG(transport_config);
    int err;
    k_tid_t tid;

    if (lora->ctx->tid) {
        return -EALREADY;
    }

    err = instance_init(lora, transport_config);

    if (err < 0) {
        return err;
    }

    tid =
        k_thread_create(lora->thread, lora->stack, CONFIG_ARES_LORA_STACK_SIZE,
                        lora_thread, (void *)lora, NULL, NULL,
                        CONFIG_ARES_LORA_PRIO, K_ESSENTIAL, K_NO_WAIT);
    k_thread_name_set(tid, lora->name);
    lora->ctx->tid = tid;

    return 0;
}

int ares_lora_register_command_callbacks(
    const struct ares_lora *lora, const struct ares_lora_command *commands,
    size_t num_commands) {
    if (lora == NULL || lora->ctx == NULL ||
        (commands == NULL && num_commands != 0)) {
        return -EINVAL;
    }

    lora->ctx->commands = commands;
    lora->ctx->num_commands = num_commands;

    return 0;
}

static void dispatch(const struct ares_lora *lora, int start_index,
                     size_t length) {
    struct ares_packet packet;
    int ret;

    ret = deserialize_ares_packet(&packet, &lora->ctx->tx_buf.buf[start_index],
                                  length);

    if (ret < 0) {
        LOG_ERR("Unable to deserialize frame");
        return;
    }

    for (size_t i = 0; i < lora->ctx->num_commands; i++) {
        if (lora->ctx->commands[i].type != packet.payload.type) {
            continue;
        }

        if (lora->ctx->commands[i].handler != NULL) {
            lora->ctx->commands[i].handler(lora, &packet);
        } else {
            LOG_WRN("LoRa command not implemented: %d",
                    lora->ctx->commands[i].type);
        }

        return;
    }

    LOG_ERR("LoRa command not found: %d", packet.payload.type);
}

static void drop_data(const struct ares_lora *lora, struct ares_lora_buf *buf) {
    size_t count;

    buf->len = 0;

    do {
        (void)LORA_API_CALL(lora, read, &buf->buf, sizeof(buf->buf), &count);
    } while (count != 0);
}

static void ares_lora_process(const struct ares_lora *lora) {
    __ASSERT_NO_MSG(lora);
    __ASSERT_NO_MSG(lora->ctx);

    size_t count = 0;
    char data;
    struct ares_lora_buf *buf = &lora->ctx->rx_buf;
    struct ares_packet_info info = {-1, -1, -1};

    while (true) {
        if (buf->len >= sizeof(buf->buf)) {
            LOG_ERR("Dropping data");
            drop_data(lora, buf);
            return;
        }

        (void)LORA_API_CALL(lora, read, &data, 1, &count);
        if (count == 0) {
            return;
        }
        LOG_DBG("Read byte: %d | 0x%X | '%c'", (int)data, (uint16_t)data, data);

        buf->buf[buf->len] = data;
        buf->len++;

        if (data == ARES_PACKET_FOOTER_1 &&
            buf->len >= ARES_PACKET_BROADCAST_OVERHEAD) {
            int result = ares_packet_present(buf->buf, buf->len, &info);
            __ASSERT_NO_MSG(result != -EINVAL);

            if (result == 0) {
                LOG_DBG(
                    "Frame info states: {start: %d, length: %d, remaining: %d}",
                    info.start, info.size, info.bytes_left);
                continue;
            }

            LOG_DBG("Packet found, Packet Length: %d", info.size);
            dispatch(lora, info.start, info.size);

            (void)memset(buf->buf, 0, buf->len);
            buf->len = 0;
            info.start = -1;
            info.size = -1;
            info.bytes_left = -1;
        }
    }
}

static int ares_lora_write(const struct ares_lora *lora, const void *data,
                           size_t nbytes) {
    __ASSERT_NO_MSG(lora);
    __ASSERT_NO_MSG(lora->ctx);
    __ASSERT_NO_MSG(data);
    __ASSERT_NO_MSG(nbytes);

    size_t offset = 0u, temp_cnt, length = nbytes;
    int err = 0;

    (void)k_mutex_lock(&lora->ctx->wr_mtx, K_FOREVER);
    while (length != 0) {
        err = LORA_API_CALL(lora, write, &((const uint8_t *)data)[offset],
                                length, &temp_cnt);
        if (err != 0) {
            break;
        }

        __ASSERT_NO_MSG(nbytes >= length);

        offset += temp_cnt;
        length -= temp_cnt;
    }
    (void)k_mutex_unlock(&lora->ctx->wr_mtx);

    if (err != 0) {
        if (length != nbytes) {
            return offset;
        }
        return err;
    }

    return (int)nbytes;
}

static int ares_lora_write_txbuf(const struct ares_lora *lora) {
    return ares_lora_write(lora, lora->ctx->tx_buf.buf, lora->ctx->tx_buf.len);
}

int ares_lora_write_packet(const struct ares_lora *lora,
                           const struct ares_packet *packet) {
    int ret;
    if (lora == NULL || packet == NULL) {
        return -EINVAL;
    }

    ret = serialize_ares_packet(lora->ctx->tx_buf.buf,
                                sizeof(lora->ctx->tx_buf.buf), packet);

    if (ret < 0) {
        return ret;
    }
    lora->ctx->tx_buf.len = ret;

    ret = ares_lora_write_txbuf(lora);
    return (ret < 0) ? ret : 0;
}

int ares_lora_configure_lora(const struct ares_lora *lora,
                             const struct lora_modem_config *config) {
    int ret;

    if (lora == NULL || config == NULL) {
        return -EINVAL;
    }

    (void)k_mutex_lock(&lora->ctx->wr_mtx, K_FOREVER);
    ret = LORA_API_CALL(lora, configure, config);
    (void)k_mutex_unlock(&lora->ctx->wr_mtx);

    return ret;
}

int ares_lora_get_new_packet_id(const struct ares_lora *lora, uint16_t *id) {
    if (lora == NULL || id == NULL) {
        return -EINVAL;
    }

    *id = lora->ctx->packet_id;
    lora->ctx->packet_id++;

    return 0;
}
