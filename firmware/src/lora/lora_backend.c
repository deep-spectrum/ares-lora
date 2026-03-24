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

static void ares_lora_transmit_handle(struct k_work *work) {
    // todo: implement this
}
// todo: create work queue for lora. Set to a lower prio

static void async_init(struct lora_async_driven *lora) {
    const struct device *dev = lora->common.dev;

    ring_buf_init(&lora->rx_ringbuf, LORA_BACKEND_RX_RINGBUF_SIZE,
                  lora->rx_buf);
    ring_buf_init(&lora->tx_ringbuf, LORA_BACKEND_TX_RINGBUF_SIZE,
                  lora->tx_buf);
    atomic_clear(&lora->tx_busy);
    lora_recv_async(dev, ares_lora_rx_handle, lora);
    // todo: tx stuff
}

static int init(const struct ares_lora_transport *transport, const void *config,
                lora_transport_handler_t evt_handler, void *context) {
    struct lora_common *common = transport->ctx;

    // todo: configure lora
    common->dev = (const struct device *)config;
    common->handler = evt_handler;
    common->context = context;

    async_init(transport->ctx);

    return 0;
}
