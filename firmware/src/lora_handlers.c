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

#define CHECK_DIRECTED_PACKET(packet)                                          \
    do {                                                                       \
        if ((packet)->type == ARES_PKT_TYPE_DIRECT &&                          \
            ((packet)->destination_id != modem_id.id ||                        \
             (packet)->pan_id != modem_id.pan_id)) {                           \
            return;                                                            \
        }                                                                      \
    } while (0)

#define DIRECTED_PACKET_REQUIRED(packet)                                       \
    do {                                                                       \
        if ((packet)->type != ARES_PKT_TYPE_DIRECT ||                          \
            ((packet)->destination_id != modem_id.id ||                        \
             (packet)->pan_id != modem_id.pan_id)) {                           \
            return;                                                            \
        }                                                                      \
    } while (0)

static void handle_start(const struct ares_lora *lora,
                         const struct ares_packet *packet) {
    ARG_UNUSED(lora);

    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_START,
    };

    CHECK_DIRECTED_PACKET(packet);

    frame.payload.START.id = packet->source_id;
    frame.payload.START.broadcast = packet->type == ARES_PKT_TYPE_BROADCAST;
    frame.payload.START.sec = packet->payload.payload.START.sec;
    frame.payload.START.usec = packet->payload.payload.START.usec;
    frame.payload.START.seq_cnt = packet->sequence_cnt;
    frame.payload.START.packet_id = packet->packet_id;

    ares_serial_write_frame(serial, &frame);
}

static void handle_poll(const struct ares_lora *lora,
                        const struct ares_packet *packet) {
    ARG_UNUSED(lora);

    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_POLL,
        .payload.POLL = packet->source_id,
    };

    DIRECTED_PACKET_REQUIRED(packet);

    ares_serial_write_frame(serial, &frame);
}

static void handle_heartbeat(const struct ares_lora *lora,
                             const struct ares_packet *packet) {
    ARG_UNUSED(lora);

    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_HEARTBEAT,
    };

    DIRECTED_PACKET_REQUIRED(packet);

    frame.payload.HEARTBEAT.flags.ready =
        packet->payload.payload.HEARTBEAT.ready;
    frame.payload.HEARTBEAT.id = packet->source_id;

    ares_serial_write_frame(serial, &frame);
}

static void handle_log(const struct ares_lora *lora,
                       const struct ares_packet *packet) {
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_LOG,
        .payload.LOG =
            {
                .broadcast = packet->type == ARES_PKT_TYPE_BROADCAST,
                .id = packet->source_id,
                .part = packet->payload.payload.LOG.part,
                .num_parts = packet->payload.payload.LOG.num_parts,
                .log_id = packet->payload.payload.LOG.log_id,
                .msg = packet->payload.payload.LOG.msg,
                .msg_len = packet->payload.payload.LOG.msg_len,
            },
    };

    CHECK_DIRECTED_PACKET(packet);

    int ret = ares_serial_write_frame(serial, &frame);
    if (ret < 0) {
        frame.type = ARES_FRAME_DBG;
        frame.payload.DBG = ret;
        ares_serial_write_frame(serial, &frame);
    }
}

static void handle_log_ack(const struct ares_lora *lora,
                           const struct ares_packet *packet) {
    ARG_UNUSED(lora);
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_LOG_ACK,
        .payload.LOG_ACK =
            {
                .part = packet->payload.payload.LOG_ACK.part,
                .num_parts = packet->payload.payload.LOG_ACK.num_parts,
                .id = packet->source_id,
                .log_id = packet->payload.payload.LOG_ACK.log_id,
            },
    };

    DIRECTED_PACKET_REQUIRED(packet);

    ares_serial_write_frame(serial, &frame);
}

static void handle_ack(const struct ares_lora *lora,
                       const struct ares_packet *packet) {
    ARG_UNUSED(lora);
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_LORA_ACK,
        .payload.LORA_ACK =
            {
                .id = packet->source_id,
                .payload_type = packet->payload.payload.ACK,
            },
    };

    DIRECTED_PACKET_REQUIRED(packet);

    ares_serial_write_frame(serial, &frame);
}

static void handle_abort(const struct ares_lora *lora,
                         const struct ares_packet *packet) {
    ARG_UNUSED(lora);
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_ABORT,
        .payload.ABORT =
            {
                .id = packet->source_id,
                .flags.broadcast = packet->type == ARES_PKT_TYPE_BROADCAST,
            },
    };

    CHECK_DIRECTED_PACKET(packet);

    ares_serial_write_frame(serial, &frame);
}

static void handle_config(const struct ares_lora *lora,
                          const struct ares_packet *packet) {
    ARG_UNUSED(lora);
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_NODE_CONFIG,
        .payload.NODE_CONFIG =
            {
                .type = packet->payload.payload.CONFIG_.type,
                .id = packet->source_id,
                .config = packet->payload.payload.CONFIG_.config,
            },
    };

    DIRECTED_PACKET_REQUIRED(packet);

    ares_serial_write_frame(serial, &frame);
}

static void handle_config_poll(const struct ares_lora *lora,
                               const struct ares_packet *packet) {
    ARG_UNUSED(lora);
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_NODE_CONFIG_POLL,
        .payload.NODE_CONFIG_POLL =
            {
                .type = packet->payload.payload.CONFIG_POLL_,
                .id = packet->source_id,
            },
    };

    DIRECTED_PACKET_REQUIRED(packet);

    ares_serial_write_frame(serial, &frame);
}

static void handle_config_poll_response(const struct ares_lora *lora,
                                        const struct ares_packet *packet) {
    ARG_UNUSED(lora);
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_NODE_CONFIG_RESP,
        .payload.NODE_CONFIG_RESP =
            {
                .id = packet->source_id,
                .config = packet->payload.payload.CONFIG_RESP_.config,
                .type = packet->payload.payload.CONFIG_RESP_.type,
            },
    };

    DIRECTED_PACKET_REQUIRED(packet);

    ares_serial_write_frame(serial, &frame);
}

static void handle_ready(const struct ares_lora *lora,
                         const struct ares_packet *packet) {
    ARG_UNUSED(lora);
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();
    struct ares_frame frame = {
        .type = ARES_FRAME_NODE_READY,
        .payload.NODE_READY =
            {
                .id = packet->source_id,
                .flags.broadcast = packet->type == ARES_PKT_TYPE_BROADCAST,
            },
    };

    CHECK_DIRECTED_PACKET(packet);

    ares_serial_write_frame(serial, &frame);
}

static struct ares_lora_command commands[] = {
    {ARES_PKT_PAYLOAD_START, handle_start},
    {ARES_PKT_PAYLOAD_POLL, handle_poll},
    {ARES_PKT_PAYLOAD_HEARTBEAT, handle_heartbeat},
    {ARES_PKT_PAYLOAD_LOG, handle_log},
    {ARES_PKT_PAYLOAD_LOG_ACK, handle_log_ack},
    {ARES_PKT_PAYLOAD_ACK, handle_ack},
    {ARES_PKT_PAYLOAD_ABORT, handle_abort},
    {ARES_PKT_PAYLOAD_CONFIG, handle_config},
    {ARES_PKT_PAYLOAD_CONFIG_POLL, handle_config_poll},
    {ARES_PKT_PAYLOAD_CONFIG_RESP, handle_config_poll_response},
    {ARES_PKT_PAYLOAD_READY, handle_ready},
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
