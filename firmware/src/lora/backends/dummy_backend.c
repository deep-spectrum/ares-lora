/**
 * @file dummy_backend.c
 *
 * @brief
 *
 * @date 11/11/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <lora/lora_backend.h>

static int dummy_init(const struct ares_lora_transport *transport,
                      const void *config, lora_transport_handler_t evt_handler,
                      void *context) {
    return 0;
}

static int dummy_uninit(const struct ares_lora_transport *transport) {
    return 0;
}

static int dummy_enable(const struct ares_lora_transport *transport,
                        bool block_tx) {
    return 0;
}

static int dummy_write(const struct ares_lora_transport *transport,
                       const void *data, size_t length, size_t *cnt) {
    *cnt = length;
    return 0;
}

static int dummy_read(const struct ares_lora_transport *transport, void *data,
                      size_t length, size_t *cnt) {
    *cnt = 0;
    return 0;
}

static int dummy_configure(const struct ares_lora_transport *transport,
                           const struct lora_modem_config *config) {
    return 0;
}

const struct ares_lora_transport_api ares_lora_transport_api = {
    .init = dummy_init,
    .uninit = dummy_uninit,
    .enable = dummy_enable,
    .write = dummy_write,
    .read = dummy_read,
    .configure = dummy_configure,
};

LORA_DEFINE(ares_lora_transport_);
ARES_LORA_DEFINE(ares_lora, &ares_lora_transport_);

static int enable_ares_lora(void) { return ares_lora_init(&ares_lora, NULL); }
SYS_INIT(enable_ares_lora, POST_KERNEL, 91);

const struct ares_lora *ares_lora_backend_lora_get_ptr(void) {
    return &ares_lora;
}
