/**
 * @file serial_backend.c
 *
 * @brief
 *
 * @date 3/20/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <serial/serial_backend.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
#include <zephyr/usb/usb_device.h>
#else
#include <sample_usbd.h>
#endif

LOG_MODULE_REGISTER(backend_uart);

enum {
    BLOCK_NO_USB_HOST,
};

static void ares_uart_rx_handle(const struct device *dev,
                                struct serial_uart_int_driven *uart) {
    uint8_t *data;
    uint32_t len;
    uint32_t rd_len;
    bool new_data = false;

    do {
        len =
            ring_buf_put_claim(&uart->rx_ringbuf, &data, uart->rx_ringbuf.size);

        if (len > 0) {
            rd_len = uart_fifo_read(dev, data, (int)len);

            if (rd_len > 0) {
                new_data = true;
            }

            int err = ring_buf_put_finish(&uart->rx_ringbuf, rd_len);
            ARG_UNUSED(err);
            __ASSERT_NO_MSG(err == 0);
        } else {
            uint8_t dummy;

            LOG_WRN("RX ring buffer full");

            rd_len = uart_fifo_read(dev, &dummy, 1);
        }
    } while (rd_len != 0);

    if (new_data) {
        uart->common.handler(SERIAL_TRANSPORT_EVT_RX_RDY, uart->common.context);
    }
}

static bool ares_uart_dtr_check(const struct device *dev) {
    int err;
    uint32_t dtr;

    err = uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
    if (err == -ENOSYS || err == -ENOTSUP) {
        return true;
    }

    return dtr;
}

static void dtr_timer_handler(struct k_timer *timer) {
    struct serial_uart_int_driven *uart = k_timer_user_data_get(timer);

    if (!ares_uart_dtr_check(uart->common.dev)) {
        return;
    }

    k_timer_stop(timer);
    uart_irq_tx_enable(uart->common.dev);
}

static void ares_uart_tx_handle(const struct device *dev,
                                struct serial_uart_int_driven *uart) {
    uint32_t len;
    const uint8_t *data;

    if (!ares_uart_dtr_check(dev)) {
        uart_irq_tx_disable(dev);
        k_timer_start(&uart->dtr_timer, K_MSEC(100), K_MSEC(100));
        return;
    }

    len = ring_buf_get_claim(&uart->tx_ringbuf, (uint8_t **)&data,
                             uart->tx_ringbuf.size);

    if (len != 0) {
        int err;

        len = uart_fifo_fill(dev, data, len);
        err = ring_buf_get_finish(&uart->tx_ringbuf, len);
        __ASSERT_NO_MSG(err == 0);
        ARG_UNUSED(err);
    } else {
        uart_irq_tx_disable(dev);
        atomic_clear(&uart->tx_busy);
    }

    uart->common.handler(SERIAL_TRANSPORT_EVT_TX_RDY, uart->common.context);
}

static void uart_callback(const struct device *dev, void *user_data) {
    struct serial_uart_int_driven *uart = user_data;

    uart_irq_update(dev);

    if (uart_irq_rx_ready(dev) != 0) {
        ares_uart_rx_handle(dev, uart);
    }

    if (uart_irq_tx_ready(dev) != 0) {
        ares_uart_tx_handle(dev, uart);
    }
}

static void irq_init(struct serial_uart_int_driven *uart) {
    const struct device *dev = uart->common.dev;

    ring_buf_init(&uart->rx_ringbuf, SERIAL_BACKEND_RX_RINGBUF_SIZE,
                  uart->rx_buf);
    ring_buf_init(&uart->tx_ringbuf, SERIAL_BACKEND_TX_RINGBUF_SIZE,
                  uart->tx_buf);
    atomic_clear(&uart->tx_busy);
    uart_irq_callback_user_data_set(dev, uart_callback, uart);
    uart_irq_rx_enable(dev);

    k_timer_init(&uart->dtr_timer, dtr_timer_handler, NULL);
    k_timer_user_data_set(&uart->dtr_timer, uart);
}

static int init(const struct ares_serial_transport *transport,
                const void *config, serial_transport_handler_t evt_handler,
                void *context) {
    struct serial_uart_common *common = transport->ctx;

    common->dev = (const struct device *)config;
    common->handler = evt_handler;
    common->context = context;

    irq_init(transport->ctx);

    return 0;
}

static void irq_uninint(struct serial_uart_int_driven *uart) {
    const struct device *dev = uart->common.dev;

    k_timer_stop(&uart->dtr_timer);
    uart_irq_tx_disable(dev);
    uart_irq_rx_disable(dev);
}

static int uninit(const struct ares_serial_transport *transport) {
    irq_uninint(transport->ctx);
    return 0;
}

static int enable(const struct ares_serial_transport *transport,
                  bool block_tx) {
    struct serial_uart_common *uart = transport->ctx;

    uart->block_tx = block_tx;
    uart_irq_tx_disable(uart->dev);
    return 0;
}

static int polling_write(struct serial_uart_common *uart, const void *data,
                         size_t length, size_t *cnt) {
    const uint8_t *data8 = data;

    for (size_t i = 0; i < length; i++) {
        uart_poll_out(uart->dev, data8[i]);
    }

    *cnt = length;
    uart->handler(SERIAL_TRANSPORT_EVT_TX_RDY, uart->context);

    return 0;
}

static int irq_write(struct serial_uart_int_driven *uart, const void *data,
                     size_t length, size_t *cnt) {
    *cnt = ring_buf_put(&uart->tx_ringbuf, data, length);

    if (atomic_set(&uart->tx_busy, 1) == 0) {
        uart_irq_tx_enable(uart->common.dev);
    }

    return 0;
}

static int write_uart(const struct ares_serial_transport *transport,
                      const void *data, size_t length, size_t *cnt) {
    struct serial_uart_common *uart = transport->ctx;

    if (!atomic_test_bit(&uart->block_no_usb, BLOCK_NO_USB_HOST) &&
        !ares_uart_dtr_check(uart->dev)) {
        // No USB host and should not block if there isn't one. Discard data and
        // move on...
        *cnt = length;
        return 0;
    }

    if (uart->block_tx) {
        return polling_write(uart, data, length, cnt);
    }

    return irq_write(transport->ctx, data, length, cnt);
}

static int irq_read(struct serial_uart_int_driven *uart, void *data,
                    size_t length, size_t *cnt) {
    *cnt = ring_buf_get(&uart->rx_ringbuf, data, length);
    return 0;
}

static int read_uart(const struct ares_serial_transport *transport, void *data,
                     size_t length, size_t *cnt) {
    return irq_read(transport->ctx, data, length, cnt);
}

static void wait_dtr(const struct ares_serial_transport *transport) {
    const struct serial_uart_common *uart = transport->ctx;
    uint32_t dtr = 0;

    while (!dtr) {
        uart_line_ctrl_get(uart->dev, UART_LINE_CTRL_DTR, &dtr);
        k_sleep(K_MSEC(100));
    }
}

static bool check_uart_error(const struct ares_serial_transport *transport) {
    const struct serial_uart_common *uart = transport->ctx;
    int err;

    err = uart_err_check(uart->dev);
    return (err != 0 && (err != -ENOSYS));
}

static void set_block_no_usb_host(const struct ares_serial_transport *transport,
                                  bool block) {
    struct serial_uart_common *uart = transport->ctx;

    if (block) {
        atomic_set_bit(&uart->block_no_usb, BLOCK_NO_USB_HOST);
    } else {
        atomic_clear_bit(&uart->block_no_usb, BLOCK_NO_USB_HOST);
    }
}

const struct ares_serial_transport_api ares_serial_uart_transport_api = {
    .init = init,
    .uninit = uninit,
    .enable = enable,
    .write = write_uart,
    .read = read_uart,
    .wait_dtr = wait_dtr,
    .rx_error = check_uart_error,
    .block_no_usb_host = set_block_no_usb_host,
};

SERIAL_UART_DEFINE(ares_serial_transport_uart);
ARES_SERIAL_DEFINE(ares_uart, &ares_serial_transport_uart);

#if !IS_ENABLED(CONFIG_USB_DEVICE_STACK)
static struct usbd_context *sample_usbd;

static void sample_msg_cb(struct usbd_context *const ctx,
                          const struct usbd_msg *msg) {
    LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

    if (usbd_can_detect_vbus(ctx)) {
        if (msg->type == USBD_MSG_VBUS_READY) {
            if (usbd_enable(ctx)) {
                LOG_ERR("Failed to enable device support");
            }
        }

        if (msg->type == USBD_MSG_VBUS_REMOVED) {
            if (usbd_disable(ctx)) {
                LOG_ERR("Failed to disable device support");
            }
        }
    }
}
#endif

static int enable_ares_serial_uart(void) {
    const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    int ret;

    if (!device_is_ready(dev)) {
        return -ENODEV;
    }

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    ret = usb_enable(NULL);
    if (ret != 0) {
        return ret;
    }
#else
    sample_usbd = sample_usbd_init_device(sample_msg_cb);
    if (sample_usbd == NULL) {
        LOG_ERR("Failed to initialize USB device");
        return -ENODEV;
    }

    if (!usbd_can_detect_vbus(sample_usbd)) {
        ret = usbd_enable(sample_usbd);
        if (ret != 0) {
            LOG_ERR("Failed to enable device support");
            return ret;
        }
    }
#endif

    return ares_serial_init(&ares_uart, dev);
}
SYS_INIT(enable_ares_serial_uart, POST_KERNEL, 90);

const struct ares_serial *ares_serial_backend_uart_get_ptr(void) {
    return &ares_uart;
}
