/**
 * @file serial_commands.c
 *
 * @brief
 *
 * @date 3/30/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ble/ble.h>
#include <led.h>
#include <lora/lora.h>
#include <lora/lora_backend.h>
#include <lora/packet.h>
#include <lora_handlers.h>
#include <ncs_version.h>
#include <serial/frame.h>
#include <serial/serial.h>
#include <settings.h>
#include <zephyr/app_version.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/version.h>

static void reboot_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    sys_reboot(SYS_REBOOT_COLD);
}
K_WORK_DELAYABLE_DEFINE(reboot_work, reboot_work_handler);

static void send_ack_frame(const struct ares_serial *serial,
                           struct ares_frame *frame, int code) {
    frame->type = ARES_FRAME_ACK;
    frame->payload.ACK = -code;
    ares_serial_write_frame(serial, frame);
}

static void send_lora_transmission(const struct ares_serial *serial,
                                   struct ares_frame *frame,
                                   struct ares_packet *packet,
                                   int64_t tx_count) {
    uint32_t id, pan, rep_cnt = (uint32_t)tx_count;
    const struct ares_lora *lora = ares_lora_backend_lora_get_ptr();
    int ret;

    ret = retrieve_setting(ARES_SETTING_ID, &id);
    if (ret < 0) {
        send_ack_frame(serial, frame, ret);
        return;
    }

    if (id == UINT32_C(0)) {
        send_ack_frame(serial, frame, -ENODEV);
        return;
    }

    ret = retrieve_setting(ARES_SETTING_PANID, &pan);
    if (ret < 0) {
        send_ack_frame(serial, frame, ret);
        return;
    }

    if (tx_count < 0) {
        ret = retrieve_setting(ARES_SETTING_REPCNT, &rep_cnt);
        if (ret < 0) {
            send_ack_frame(serial, frame, ret);
            return;
        }
    }

    packet->pan_id = (uint16_t)pan;
    packet->source_id = (uint16_t)id;
    (void)ares_lora_set_packet_id(lora, packet);

    uint32_t send_count = 0u;
    for (size_t i = 0; i < rep_cnt; i++) {
        ret = ares_lora_write_packet(lora, packet);
        if (ret < 0) {
            send_ack_frame(serial, frame, ret);
            return;
        }
        send_count++;
    }

    send_ack_frame(serial, frame, 0);

#if IS_ENABLED(CONFIG_ARES_LORA_NOTIF_RX_PACKETS)
    frame->type = ARES_FRAME_PKT_TX;
    frame->payload.PKT_TX = send_count;
    (void)ares_serial_write_frame(serial, frame);
#endif
}

ARES_FRAME_HANDLER(ARES_FRAME_SETTING) {
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

ARES_FRAME_HANDLER(ARES_FRAME_START) {
    struct ares_packet packet = {
        .payload = {.type = ARES_PKT_PAYLOAD_START,
                    .payload.START =
                        {
                            .sec = frame->payload.START.sec,
                            .usec = frame->payload.START.usec,
                        }},
        .destination_id = frame->payload.START.id,
        .type = (frame->payload.START.broadcast) ? ARES_PKT_TYPE_BROADCAST
                                                 : ARES_PKT_TYPE_DIRECT,
    };

    send_lora_transmission(serial, frame, &packet,
                           (frame->payload.START.broadcast) ? -1 : 1);
}

ARES_FRAME_HANDLER(ARES_FRAME_LORA_CONFIG) {
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

ARES_FRAME_HANDLER(ARES_FRAME_LED) {
    int ret =
        update_led_state(frame->payload.LED.led, frame->payload.LED.state);

    if (frame->payload.LED.state >= LED_STATE_INVALID) {
        frame->payload.LED.state = (uint8_t)ret;
        ares_serial_write_frame(serial, frame);
        return;
    }

    send_ack_frame(serial, frame, ret);
}

ARES_FRAME_HANDLER(ARES_FRAME_HEARTBEAT) {
    struct ares_packet packet = {
        .type = ARES_PKT_TYPE_DIRECT,
        .destination_id = frame->payload.HEARTBEAT.id,
        .payload = {.type = ARES_PKT_PAYLOAD_HEARTBEAT,
                    .payload.HEARTBEAT = {
                        .ready = frame->payload.HEARTBEAT.flags.ready,
                    }}};

    send_lora_transmission(serial, frame, &packet, 1);
}

ARES_FRAME_HANDLER(ARES_FRAME_POLL) {
    struct ares_packet packet = {.type = ARES_PKT_TYPE_DIRECT,
                                 .destination_id = frame->payload.POLL,
                                 .payload = {
                                     .type = ARES_PKT_PAYLOAD_POLL,
                                 }};

    send_lora_transmission(serial, frame, &packet, 1);
}

ARES_FRAME_HANDLER(ARES_FRAME_LOG) {
    struct ares_packet packet = {
        .type = frame->payload.LOG.broadcast ? ARES_PKT_TYPE_BROADCAST
                                             : ARES_PKT_TYPE_DIRECT,
        .destination_id = frame->payload.LOG.id,
        .payload = {.type = ARES_PKT_PAYLOAD_LOG,
                    .payload.LOG =
                        {
                            .part = frame->payload.LOG.part,
                            .num_parts = frame->payload.LOG.num_parts,
                            .log_id = frame->payload.LOG.log_id,
                            .msg = frame->payload.LOG.msg,
                            .msg_len = frame->payload.LOG.msg_len,
                        }},
    };

    send_lora_transmission(serial, frame, &packet, frame->payload.LOG.tx_cnt);
}

ARES_FRAME_HANDLER(ARES_FRAME_LOG_ACK) {
    struct ares_packet packet = {
        .type = ARES_PKT_TYPE_DIRECT,
        .destination_id = frame->payload.LOG_ACK.id,
        .payload = {.type = ARES_PKT_PAYLOAD_LOG_ACK,
                    .payload.LOG_ACK = {
                        .log_id = frame->payload.LOG_ACK.log_id,
                        .part = frame->payload.LOG_ACK.part,
                        .num_parts = frame->payload.LOG_ACK.num_parts,
                    }}};

    send_lora_transmission(serial, frame, &packet, 1);
}

ARES_FRAME_HANDLER(ARES_FRAME_VERSION) {
    frame->payload.VERSION.app = (uint32_t)APP_VERSION_NUMBER;
    frame->payload.VERSION.kernel = (uint32_t)KERNEL_VERSION_NUMBER;
    frame->payload.VERSION.ncs = (uint32_t)NCS_VERSION_NUMBER;

    ares_serial_write_frame(serial, frame);
}

static int helper_ble_enable(void) {
    uint32_t id = 0;

    int ret = retrieve_setting(ARES_SETTING_ID, &id);
    if (ret < 0) {
        return ret;
    }

    if (id == 0) {
        return -ENODEV;
    }

    ret = ares_set_ble_node(id);
    if (ret < 0) {
        return ret;
    }

    return ares_enable_ble();
}

ARES_FRAME_HANDLER(ARES_FRAME_BLE_STATE) {
    switch (frame->payload.BLE_STATE) {
    case 0: {
        frame->type = ARES_FRAME_ACK;
        frame->payload.ACK = ares_disable_ble();
        break;
    }
    case 1: {
        frame->type = ARES_FRAME_ACK;
        frame->payload.ACK = helper_ble_enable();
        break;
    }
    case 2: {
        frame->payload.BLE_STATE = ares_ble_enabled() != false;
        break;
    }
    default: {
        frame->type = ARES_FRAME_ACK;
        frame->payload.ACK = EINVAL;
        break;
    }
    }

    ares_serial_write_frame(serial, frame);
}

ARES_FRAME_HANDLER(ARES_FRAME_BLE_DISCONNECT) {
    send_ack_frame(serial, frame, ares_disconnect_ble());
}

ARES_FRAME_HANDLER(ARES_FRAME_REBOOT) {
    const uint8_t min = 5, max = 30;
    uint8_t reboot_time = frame->payload.REBOOT;
    int ret;

    if (reboot_time < min) {
        reboot_time = min;
    } else if (reboot_time > max) {
        reboot_time = max;
    }

    ret = k_work_schedule(&reboot_work, K_SECONDS(reboot_time));

    if (ret > 0) {
        ret = 0;
    }

    send_ack_frame(serial, frame, ret);
}

ARES_FRAME_HANDLER(ARES_FRAME_LORA_ACK) {
    struct ares_packet packet = {
        .type = ARES_PKT_TYPE_DIRECT,
        .destination_id = frame->payload.LORA_ACK.id,
        .payload = {.type = ARES_PKT_PAYLOAD_ACK,
                    .payload.ACK = frame->payload.LORA_ACK.payload_type},
    };

    send_lora_transmission(serial, frame, &packet, 1);
}

ARES_FRAME_HANDLER(ARES_FRAME_ABORT) {
    struct ares_packet packet = {.type = (frame->payload.ABORT.flags.broadcast)
                                             ? ARES_PKT_TYPE_BROADCAST
                                             : ARES_PKT_TYPE_DIRECT,
                                 .destination_id = frame->payload.ABORT.id,
                                 .payload = {
                                     .type = ARES_PKT_PAYLOAD_ABORT,
                                 }};

    send_lora_transmission(serial, frame, &packet,
                           (frame->payload.ABORT.flags.broadcast) ? -1 : 1);
}

ARES_FRAME_HANDLER(ARES_FRAME_NODE_CONFIG) {
    struct ares_packet packet = {
        .type = ARES_PKT_TYPE_DIRECT,
        .destination_id = frame->payload.NODE_CONFIG.id,
        .payload =
            {
                .type = ARES_PKT_PAYLOAD_CONFIG,
                .payload.CONFIG_ =
                    {
                        .type = frame->payload.NODE_CONFIG.type,
                        .config = frame->payload.NODE_CONFIG.config,
                    },
            },
    };

    send_lora_transmission(serial, frame, &packet, 1);
}

ARES_FRAME_HANDLER(ARES_FRAME_NODE_CONFIG_POLL) {
    struct ares_packet packet = {
        .type = ARES_PKT_TYPE_DIRECT,
        .destination_id = frame->payload.NODE_CONFIG_POLL.id,
        .payload =
            {
                .type = ARES_PKT_PAYLOAD_CONFIG_POLL,
                .payload.CONFIG_POLL_ = frame->payload.NODE_CONFIG_POLL.type,
            },
    };

    send_lora_transmission(serial, frame, &packet, 1);
}

ARES_FRAME_HANDLER(ARES_FRAME_NODE_CONFIG_RESP) {
    struct ares_packet packet = {
        .type = ARES_PKT_TYPE_DIRECT,
        .destination_id = frame->payload.NODE_CONFIG_RESP.id,
        .payload =
            {
                .type = ARES_PKT_PAYLOAD_CONFIG_RESP,
                .payload.CONFIG_RESP_ =
                    {
                        .type = frame->payload.NODE_CONFIG_RESP.type,
                        .config = frame->payload.NODE_CONFIG_RESP.config,
                    },
            },
    };

    send_lora_transmission(serial, frame, &packet, 1);
}

ARES_FRAME_HANDLER(ARES_FRAME_NODE_READY) {
    struct ares_packet packet = {
        .type = (frame->payload.NODE_READY.flags.broadcast)
                    ? ARES_PKT_TYPE_BROADCAST
                    : ARES_PKT_TYPE_DIRECT,
        .destination_id = frame->payload.NODE_READY.id,
        .payload =
            {
                .type = ARES_PKT_PAYLOAD_READY,
            },
    };

    send_lora_transmission(serial, frame, &packet,
                           (frame->payload.NODE_READY.flags.broadcast) ? -1
                                                                       : 1);
}

ARES_FRAME_HANDLER(ARES_FRAME_BLE_NODE_CONFIG) {
    frame->type = ARES_FRAME_ACK;
    frame->payload.ACK =
        ares_send_config_response(frame->payload.BLE_NODE_CONFIG.type,
                                  frame->payload.BLE_NODE_CONFIG.data,
                                  frame->payload.BLE_NODE_CONFIG.data_len);
    ares_serial_write_frame(serial, frame);
}

ARES_FRAME_HANDLER(ARES_FRAME_BLE_NEIGHBOR_UPDATE) {
    int ret = ares_send_neighbor_states(
        frame->payload.BLE_NEIGHBOR_UPDATE.num_neighbors,
        frame->payload.BLE_NEIGHBOR_UPDATE.neighbor_data,
        frame->payload.BLE_NEIGHBOR_UPDATE.neighbor_data_len);

    frame->type = ARES_FRAME_ACK;
    frame->payload.ACK = ret;

    ares_serial_write_frame(serial, frame);
}

struct ble_user_data {
    const struct ares_serial *serial;
    uint16_t mtu;
    uint16_t interval;
    uint16_t latency;
    uint16_t timeout;
    uint8_t phy;
};

struct ble_user_data ares_ble_user_data;

static void connected(void *data) {
    struct ble_user_data *ctx = data;
    struct ares_frame frame = {
        .type = ARES_FRAME_BLE_CONNECTED,
        .payload.BLE_CONNECTED = true,
    };

    ares_serial_write_frame(ctx->serial, &frame);
}

static void disconnected(void *data) {
    struct ble_user_data *ctx = data;
    struct ares_frame frame = {
        .type = ARES_FRAME_BLE_CONNECTED,
        .payload.BLE_CONNECTED = false,
    };

    ares_serial_write_frame(ctx->serial, &frame);
}

static void mtu_size_change(void *data, size_t mtu) {
    struct ble_user_data *ctx = data;
    struct ares_frame frame = {.type = ARES_FRAME_BLE_CONN_PARAMS,
                               .payload.BLE_CONN_PARAMS = {
                                   .mtu = mtu,
                                   .interval = ctx->interval,
                                   .latency = ctx->latency,
                                   .phy = ctx->phy,
                                   .timeout = ctx->timeout,
                               }};

    ctx->mtu = mtu;

    ares_serial_write_frame(ctx->serial, &frame);
}

static void conn_param_updated(void *data, uint16_t interval, uint16_t latency,
                               uint16_t timeout) {
    struct ble_user_data *ctx = data;
    struct ares_frame frame = {.type = ARES_FRAME_BLE_CONN_PARAMS,
                               .payload.BLE_CONN_PARAMS = {
                                   .mtu = ctx->mtu,
                                   .interval = interval,
                                   .latency = latency,
                                   .phy = ctx->phy,
                                   .timeout = timeout,
                               }};

    ctx->interval = interval;
    ctx->latency = latency;
    ctx->timeout = timeout;

    ares_serial_write_frame(ctx->serial, &frame);
}

static void phy_updated(void *data, enum le_phy phy) {
    struct ble_user_data *ctx = data;
    struct ares_frame frame = {.type = ARES_FRAME_BLE_CONN_PARAMS,
                               .payload.BLE_CONN_PARAMS = {
                                   .mtu = ctx->mtu,
                                   .interval = ctx->interval,
                                   .latency = ctx->latency,
                                   .phy = phy,
                                   .timeout = ctx->timeout,
                               }};

    ctx->phy = phy;

    ares_serial_write_frame(ctx->serial, &frame);
}

static uint8_t check_subscription_mask(uint32_t subscriptions, uint32_t mask) {
    return (subscriptions & mask) != 0;
}

static void subscription_update(void *data, uint32_t subscriptions) {
    struct ble_user_data *ctx = data;
    struct ares_frame frame = {
        .type = ARES_FRAME_BLE_SUBSCRIBED,
        .payload.BLE_SUBSCRIBED = {
            .neighbor_states_subscribed = check_subscription_mask(
                subscriptions, ARES_SUBSCRIPTION_NEIGHBOR_UPDATES),
            .bandwidth = check_subscription_mask(subscriptions,
                                                 ARES_SUBSCRIPTION_BANDWIDTH),
            .center_freq = check_subscription_mask(
                subscriptions, ARES_SUBSCRIPTION_CENTER_FREQ),
            .ref_lvl = check_subscription_mask(subscriptions,
                                               ARES_SUBSCRIPTION_REF_LVL),
            .duration = check_subscription_mask(subscriptions,
                                                ARES_SUBSCRIPTION_DURATION),
            .description = check_subscription_mask(
                subscriptions, ARES_SUBSCRIPTION_DESCRIPTION),
        }};

    ares_serial_write_frame(ctx->serial, &frame);
}

// TODO: Combine config_update and description update under one handler
static void config_update(void *data, uint32_t type, uint64_t config) {
    struct ble_user_data *ctx = data;
    struct ares_frame frame = {.type = ARES_FRAME_BLE_NODE_CONFIG,
                               .payload.BLE_NODE_CONFIG = {
                                   .type = type,
                                   .data = &config,
                               }};

    switch (type) {
    case ARES_CONFIG_BANDWIDTH:
    case ARES_CONFIG_CENTER_FREQ:
    case ARES_CONFIG_REF_LEVEL: {
        frame.payload.BLE_NODE_CONFIG.data_len = sizeof(double);
        break;
    }
    case ARES_CONFIG_DURATION: {
        frame.payload.BLE_NODE_CONFIG.data_len = sizeof(uint32_t);
        break;
    }
    default: {
        return;
    }
    }

    ares_serial_write_frame(ctx->serial, &frame);
}

static int initialize_ble(const struct ares_serial *serial) {
    struct ares_ble_init_data init_data = {
        .cb =
            {
                .config_update = config_update,
                .description_update = NULL,
                .config_request = NULL,
                .subscription_update = subscription_update,
                .connected = connected,
                .disconnected = disconnected,
                .connection_param_updated = conn_param_updated,
                .mtu_size_changed = mtu_size_change,
                .phy_updated = phy_updated,
                .send_config_response_error = NULL,
                .start = NULL,
            },
        .user_data = &ares_ble_user_data,
    };

    ares_ble_user_data.serial = serial;

    (void)retrieve_setting(ARES_SETTING_ID, &init_data.node_id);

    return ares_init_ble(&init_data);
}

static int init_serial_handlers(void) {
    const struct ares_serial *serial = ares_serial_backend_uart_get_ptr();

    int ret = initialize_ble(serial);
    if (ret != 0) {
        return ret;
    }
    return 0;
}
SYS_INIT(init_serial_handlers, APPLICATION, 90);
