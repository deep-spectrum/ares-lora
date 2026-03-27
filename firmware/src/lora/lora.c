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
