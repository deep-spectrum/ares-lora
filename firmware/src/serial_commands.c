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

static void send_ack_frame(const struct ares_serial *serial,
                           struct ares_frame *frame, int code) {
    frame->type = ARES_FRAME_ACK;
    frame->payload.ACK = -code;
    ares_serial_write_frame(serial, frame);
}

static void handle_id(const struct ares_serial *serial,
                      struct ares_frame *frame) {
    int32_t setting;
    int ret;

    if (!frame->payload.ID.set) {
        ret = retrieve_setting(ARES_SETTING_ID, &setting);
        if (ret < 0) {
            send_ack_frame(serial, frame, ret);
            return;
        }

        frame->payload.ID.id = (uint16_t)setting;
        ares_serial_write_frame(serial, frame);
        return;
    }

    setting = frame->payload.ID.id;

    ret = update_setting(ARES_SETTING_ID, setting);
    if (ret < 0) {
        send_ack_frame(serial, frame, ret);
    }

    send_ack_frame(serial, frame, 0);
}

static void handle_start(const struct ares_serial *serial,
                         struct ares_frame *frame) {
    int32_t id, pan, rep_cnt;
    const struct ares_lora *lora = ares_lora_backend_lora_get_ptr();
    int ret;

    struct ares_packet packet = {
        .payload = {.type = ARES_PKT_PAYLOAD_START,
                    .payload.timespec =
                        {
                            .sec = frame->payload.START.sec,
                            .nsec = frame->payload.START.ns,
                        }},
        .destination_id = frame->payload.START.destination,
        .type = (frame->payload.START.broadcast) ? ARES_PKT_TYPE_BROADCAST
                                                 : ARES_PKT_TYPE_DIRECT,
    };

    ret = retrieve_setting(ARES_SETTING_ID, &id);
    if (retrieve_setting(ARES_SETTING_ID, &id) < 0) {
        send_ack_frame(serial, frame, ret);
        return;
    }

    ret = retrieve_setting(ARES_SETTING_PANID, &pan);
    if (ret < 0) {
        send_ack_frame(serial, frame, ret);
        return;
    }

    ret = retrieve_setting(ARES_SETTING_REPCNT, &rep_cnt);
    if (ret < 0) {
        send_ack_frame(serial, frame, ret);
        return;
    }

    packet.pan_id = (uint16_t)pan;
    packet.source_id = (uint16_t)id;

    for (size_t i = 0; i < (size_t)rep_cnt; i++) {
        packet.sequence_cnt = (uint8_t)i;
        ares_lora_write_packet(lora, &packet);
    }

    send_ack_frame(serial, frame, 0);
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
