/**
 * @file lora_handlers.c
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
#include <lora_handlers.h>
#include <serial/frame.h>
#include <serial/serial.h>
#include <serial/serial_backend.h>
#include <settings.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

struct modem_id {
    uint16_t pan_id;
    uint16_t id;
};

static struct modem_id modem_id;

static void handle_start(const struct ares_lora *lora,
                         const struct ares_packet *packet) {
    ARG_UNUSED(lora);

    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_START,
    };

    if (packet->type == ARES_PKT_TYPE_DIRECT &&
        (packet->destination_id != modem_id.id ||
         packet->pan_id != modem_id.pan_id)) {
        // not meant for us...
        return;
    }

    frame.payload.START.id = packet->source_id;
    frame.payload.START.broadcast = packet->type == ARES_PKT_TYPE_BROADCAST;
    frame.payload.START.sec = packet->payload.payload.START.sec;
    frame.payload.START.ns = packet->payload.payload.START.nsec;
    frame.payload.START.seq_cnt = packet->sequence_cnt;
    frame.payload.START.packet_id = packet->packet_id;

    ares_serial_write_frame(serial, &frame);
}

static struct ares_lora_command commands[] = {
    {ARES_PKT_PAYLOAD_START, handle_start},
};

static int init_lora_handlers(void) {
    const struct ares_lora *lora = ares_lora_backend_lora_get_ptr();
    refresh_modem_id();
    return ares_lora_register_command_callbacks(lora, commands,
                                                ARRAY_SIZE(commands));
}
SYS_INIT(init_lora_handlers, APPLICATION, 91);

void refresh_modem_id(void) {
    uint32_t pan, id;

    retrieve_setting(ARES_SETTING_PANID, &pan);
    retrieve_setting(ARES_SETTING_ID, &id);

    modem_id.id = (uint16_t)id;
    modem_id.pan_id = (uint16_t)pan;
}
