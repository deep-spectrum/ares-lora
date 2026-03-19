/**
 * @file lora.c
 *
 * @brief
 *
 * @date 3/19/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <lora.h>
#include <lora_frame.h>
#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lora);

const struct device *const lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));

int ares_lora_config(void) {
    struct lora_modem_config config = {};
    int ret;

    if (!device_is_ready(lora_dev)) {
        LOG_ERR("%s: device is not ready", lora_dev->name);
        return -ENODEV;
    }

    config.frequency = 915000000;
    config.bandwidth = BW_125_KHZ;
    config.datarate = SF_12;
    config.preamble_len = 8;
    config.coding_rate = CR_4_5;
    config.iq_inverted = false;
    config.public_network = false;
    config.tx_power = 4;
    config.tx = true;

    ret = lora_config(lora_dev, &config);
    if (ret < 0) {
        LOG_ERR("LoRa config failed: %d", ret);
        return ret;
    }

    LOG_INF("LoRa configured");

    return 0;
}

int ares_lora_send(const struct lora_send_data *data, uint8_t repeat_count,
                   k_timeout_t interval) {
    static uint64_t send_id = UINT64_C(0);
    uint8_t serial_frame[lora_frame_size];
    int ret;

    if (data == NULL || repeat_count == UINT8_C(0)) {
        return -EINVAL;
    }

    for (uint8_t i = 0; i < repeat_count; i++) {
        struct lora_frame frame = {
            .id = send_id,
            .seq_cnt = i,
            .second = data->second,
            .nanosecond = data->nanosecond,
        };

        ret = serialize_lora_frame(serial_frame, sizeof(serial_frame), &frame);
        if (ret < 0) {
            LOG_ERR("Failed to create LoRa frame: %d", ret);
            return ret;
        }

        // todo: DELETE THIS WHEN CHECK PASSES
        if (check_lora_frame(serial_frame, sizeof(serial_frame)) !=
            LORA_FRAME_CHECK_OK) {
            LOG_ERR("LoRa frame check doesn't work");
            break;
        }

        ret = lora_send(lora_dev, serial_frame, sizeof(serial_frame));
        if (ret < 0) {
            LOG_ERR("LoRa send failed");
            return ret;
        }

        if (i < (repeat_count - 1)) {
            (void)k_sleep(interval);
        }
    }

    return 0;
}
