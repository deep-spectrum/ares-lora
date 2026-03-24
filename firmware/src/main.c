#include <serial/serial.h>
#include <serial/serial_backend.h>
#include <zephyr/kernel.h>

static void whoami(const struct ares_serial *serial, struct ares_frame *frame) {
    struct ares_frame tx_frame = {
        .type = ARES_FRAME_WHOAMI,
        .payload.id = "Transmitter",
    };

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

int main(void) {
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();

    ares_serial_register_command_callbacks(serial, commands,
                                           ARRAY_SIZE(commands));

    return 0;
}
