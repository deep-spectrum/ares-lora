#include <lora/packet.h>
#include <serial/serial.h>
#include <serial/serial_backend.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define LED0_NODE     DT_ALIAS(led0)
#define SLEEP_TIME_MS 1000

static void whoami(const struct ares_serial *serial, struct ares_frame *frame) {
    struct ares_frame tx_frame = {
        .type = ARES_FRAME_WHOAMI,
        .payload.id = "Transmitter",
    };

    uint8_t test_buf[256];
    struct ares_packet packet = {
        .type = ARES_PKT_TYPE_BROADCAST,
        .sequence_cnt = 0,
        .pan_id = 0x1234,
        .source_id = 0x1234,
        .payload = {.type = ARES_PKT_PAYLOAD_START,
                    .payload.timespec = {.nsec = 100, .sec = 100}}};

    int ret = serialize_ares_packet(test_buf, 256, &packet);

    LOG_HEXDUMP_INF(test_buf, ret, "Packet: ");
    LOG_INF("Packet valid: %d", ares_packet_valid(test_buf, ret));

    ares_serial_write_frame(serial, &tx_frame);
}

static struct ares_serial_command commands[] = {
    {
        .command = ARES_FRAME_WHOAMI,
        .callback = whoami,
    },

    {
        .command = ARES_FRAME_START,
        .callback = NULL,
    }};

static void led_status(void) {
    int ret;
    static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

    if (!gpio_is_ready_dt(&led)) {
        return;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return;
    }

    while (1) {
        ret = gpio_pin_toggle_dt(&led);
        if (ret < 0) {
            return;
        }

        k_msleep(SLEEP_TIME_MS);
    }
}

int main(void) {
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();

    ares_serial_register_command_callbacks(serial, commands,
                                           ARRAY_SIZE(commands));

    led_status();

    return 0;
}
