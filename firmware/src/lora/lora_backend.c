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

static void ares_lora_rx_handle(const struct device *dev, uint8_t *data,
                                uint16_t size, int16_t rssi, int8_t snr,
                                void *ctx) {
    uint32_t len;
    struct lora_async_driven *lora = ctx;

    len = ring_buf_put_claim(&lora->rx_ringbuf, &data, lora->rx_ringbuf.size);

    if (len > 0) {
        int err =
            ring_buf_put_finish(&lora->rx_ringbuf, (size < len) ? size : len);
        ARG_UNUSED(err);
        __ASSERT_NO_MSG(err == 0);
    }

    lora->common.handler(LORA_TRANSPORT_EVT_RX_RDY, lora->common.context);
}
