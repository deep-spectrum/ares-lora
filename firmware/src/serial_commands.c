/**
 * @file serial_commands.c
 *
 * @brief
 *
 * @date 3/30/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <lora/lora.h>
#include <lora/lora_backend.h>
#include <lora/packet.h>
#include <serial/frame.h>
#include <serial/serial.h>
#include <settings.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

static void handle_id(const struct ares_serial *serial,
                      struct ares_frame *frame) {
    int32_t setting;
    if (!frame->payload.ID.set) {

        if (retrieve_setting(ARES_SETTING_ID, &setting) < 0) {
            // todo
            return;
        }

        frame->payload.ID.id = (uint16_t)setting;
        ares_serial_write_frame(serial, frame);
        return;
    }

    setting = frame->payload.ID.id;

    if (update_setting(ARES_SETTING_ID, setting) < 0) {
        // todo
    }
}

static void handle_start(const struct ares_serial *serial,
                         struct ares_frame *frame) {
    // todo
}

static struct ares_serial_command commands[] = {
    {ARES_FRAME_ID, handle_id},
    {ARES_FRAME_START, handle_start},
};

static int init_serial_handlers(void) {
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    return ares_serial_register_command_callbacks(serial, commands,
                                                  ARRAY_SIZE(commands));
}
SYS_INIT(init_serial_handlers, APPLICATION, 90);
