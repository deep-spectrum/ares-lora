/**
 * @file lora_backend.c
 *
 * @brief
 *
 * @date 3/24/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <lora/lora_backend.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>

static void ares_lora_rx_handle(const struct device *dev, uint8_t *rx_data,
                                uint16_t size, int16_t rssi, int8_t snr,
                                void *ctx) {
    uint32_t len;
    uint8_t *data;
    struct lora_async_driven *lora = ctx;

    len = ring_buf_put_claim(&lora->rx_ringbuf, &data, lora->rx_ringbuf.size);

    if (len > 0) {
        int err;
        size_t rx_len = ((uint32_t)size < len) ? size : len;
        (void)memcpy(data, rx_data, rx_len);

        err = ring_buf_put_finish(&lora->rx_ringbuf, rx_len);
        ARG_UNUSED(err);
        __ASSERT_NO_MSG(err == 0);
    }

    lora->common.handler(LORA_TRANSPORT_EVT_RX_RDY, lora->common.context);
}

static void async_init(struct lora_async_driven *lora) {
    const struct device *dev = lora->common.dev;

    ring_buf_init(&lora->rx_ringbuf, LORA_BACKEND_RX_RINGBUF_SIZE,
                  lora->rx_buf);
    ring_buf_init(&lora->tx_ringbuf, LORA_BACKEND_TX_RINGBUF_SIZE,
                  lora->tx_buf);
    atomic_clear(&lora->tx_busy);
    lora_recv_async(dev, ares_lora_rx_handle, lora);
}

static int configure_modem(struct lora_common *lora) {
    return lora_config(lora->dev, &lora->config);
}

static int init(const struct ares_lora_transport *transport, const void *config,
                lora_transport_handler_t evt_handler, void *context) {
    struct lora_common *common = transport->ctx;
    struct lora_modem_config *modem_config = &common->config;

    common->dev = (const struct device *)config;
    common->handler = evt_handler;
    common->context = context;

    modem_config->frequency = 915000000;
    modem_config->bandwidth = BW_125_KHZ;
    modem_config->datarate = SF_12;
    modem_config->preamble_len = 8;
    modem_config->coding_rate = CR_4_5;
    modem_config->iq_inverted = false;
    modem_config->public_network = false;
    modem_config->tx_power = 4;
    modem_config->tx = false;
    (void)configure_modem(common);

    async_init(transport->ctx);

    return 0;
}

static void async_uninit(struct lora_async_driven *lora) {
    const struct device *dev = lora->common.dev;

    lora_recv_async(dev, NULL, NULL);
}

static int uninit(const struct ares_lora_transport *transport) {
    async_uninit(transport->ctx);
    return 0;
}

static int enable(const struct ares_lora_transport *transport, bool block_tx) {
    ARG_UNUSED(transport);
    ARG_UNUSED(block_tx);
    return 0;
}

static int configure_modem_api(const struct ares_lora_transport *transport,
                               const struct lora_modem_config *config) {
    struct lora_common *lora = transport->ctx;

    lora->config = *config;
    lora->config.tx = false;

    return configure_modem(lora);
}

static int polling_write(struct lora_common *lora, const void *data,
                         size_t length, size_t *cnt) {
    uint8_t *data8 = (uint8_t *)data;

    int ret = lora_send(lora->dev, data8, length);
    if (ret < 0) {
        *cnt = 0;
        return ret;
    }

    *cnt = length;
    lora->handler(LORA_TRANSPORT_EVT_TX_RDY, lora->context);

    return 0;
}

static int lora_write(const struct ares_lora_transport *transport,
                      const void *data, size_t length, size_t *cnt) {
    struct lora_common *lora = transport->ctx;
    int ret;

    // set modem to TX mode for this message
    lora->config.tx = true;
    ret = configure_modem(lora);
    if (ret < 0) {
        return ret;
    }

    ret = polling_write(lora, data, length, cnt);

    if (ret < 0) {
        return ret;
    }

    // Leave the modem in receive mode whenever not transmitting...
    lora->config.tx = false;
    return configure_modem(lora);
}

static int async_read(struct lora_async_driven *lora, void *data, size_t length,
                      size_t *cnt) {
    *cnt = ring_buf_get(&lora->rx_ringbuf, data, length);
    return 0;
}

static int lora_read(const struct ares_lora_transport *transport, void *data,
                     size_t length, size_t *cnt) {
    return async_read(transport->ctx, data, length, cnt);
}

const struct ares_lora_transport_api ares_lora_transport_api = {
    .init = init,
    .uninit = uninit,
    .enable = enable,
    .write = lora_write,
    .read = lora_read,
    .configure = configure_modem_api,
};

LORA_DEFINE(ares_lora_transport_);
ARES_LORA_DEFINE(ares_lora, &ares_lora_transport_);

static int enable_ares_lora(void) {
    const struct device *const dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(lora0));

    if (!device_is_ready(dev)) {
        return -ENODEV;
    }

    return ares_lora_init(&ares_lora, dev);
}
SYS_INIT(enable_ares_lora, POST_KERNEL, 91);

const struct ares_lora *ares_lora_backend_lora_get_ptr(void) {
    return &ares_lora;
}
