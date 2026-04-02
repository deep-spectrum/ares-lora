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
#include <lora_handlers.h>
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

static void handle_setting(const struct ares_serial *serial,
                           struct ares_frame *frame) {
    uint32_t setting;
    int ret;

    if (!frame->payload.SETTING.set) {
        ret = retrieve_setting(frame->payload.SETTING.setting, &setting);
        if (ret < 0) {
            send_ack_frame(serial, frame, ret);
            return;
        }

        frame->payload.SETTING.value = setting;
        ares_serial_write_frame(serial, frame);
        return;
    }

    setting = frame->payload.SETTING.value;

    ret = update_setting(frame->payload.SETTING.setting, setting);
    if (ret < 0) {
        send_ack_frame(serial, frame, ret);
        return;
    }

    if (frame->payload.SETTING.setting == ARES_SETTING_ID ||
        frame->payload.SETTING.setting == ARES_SETTING_PANID) {
        refresh_modem_id();
    }

    send_ack_frame(serial, frame, 0);
}

static void handle_start(const struct ares_serial *serial,
                         struct ares_frame *frame) {
    uint32_t id, pan, rep_cnt;
    const struct ares_lora *lora = ares_lora_backend_lora_get_ptr();
    int ret;

    struct ares_packet packet = {
        .payload = {.type = ARES_PKT_PAYLOAD_START,
                    .payload.timespec =
                        {
                            .sec = frame->payload.START.sec,
                            .nsec = frame->payload.START.ns,
                        }},
        .destination_id = frame->payload.START.id,
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

    for (size_t i = 0; i < rep_cnt; i++) {
        packet.sequence_cnt = (uint8_t)i;
        ares_lora_write_packet(lora, &packet);
    }

    send_ack_frame(serial, frame, 0);
}

static void handle_lora_config(const struct ares_serial *serial,
                               struct ares_frame *frame) {
    const struct ares_lora *lora = ares_lora_backend_lora_get_ptr();
    struct lora_modem_config config = {
        .frequency = frame->payload.LORA_CONFIG.freq_hz,
        .bandwidth = frame->payload.LORA_CONFIG.bandwidth,
        .coding_rate = frame->payload.LORA_CONFIG.coding_rate,
        .datarate = frame->payload.LORA_CONFIG.data_rate,
        .preamble_len = frame->payload.LORA_CONFIG.preamble_len,
        .tx_power = frame->payload.LORA_CONFIG.tx_pow_dbm};
    int ret;

    ret = ares_lora_configure_lora(lora, &config);

    send_ack_frame(serial, frame, ret);
}

static struct ares_serial_command commands[] = {
    {ARES_FRAME_SETTING, handle_setting},
    {ARES_FRAME_START, handle_start},
    {ARES_FRAME_LORA_CONFIG, handle_lora_config},
};

static int init_serial_handlers(void) {
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    return ares_serial_register_command_callbacks(serial, commands,
                                                  ARRAY_SIZE(commands));
}
SYS_INIT(init_serial_handlers, APPLICATION, 90);
