#include <led.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define LED0_NODE     DT_ALIAS(led0)
#define SLEEP_TIME_MS 1000
#define TRY_AGAIN_MS  10
#define LED_ON        0
#define LED_OFF       1

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static enum led_state state = BLINK;
K_MUTEX_DEFINE(led_mtx);

static void blink_work(struct k_work *work) {
    struct k_work_delayable *dwork =
        CONTAINER_OF(work, struct k_work_delayable, work);

    int ret = k_mutex_lock(&led_mtx, K_NO_WAIT);
    if (ret < 0) {
        k_work_schedule(dwork, K_MSEC(TRY_AGAIN_MS));
        return;
    }

    if (state != BLINK) {
        k_mutex_unlock(&led_mtx);
        return;
    }

    (void)gpio_pin_toggle_dt(&led);
    k_mutex_unlock(&led_mtx);
    k_work_schedule(dwork, K_MSEC(SLEEP_TIME_MS));
}
K_WORK_DELAYABLE_DEFINE(blink, blink_work);

static void start_led(void) {
    int ret;

    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED GPIO not ready");
        return;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED pin");
        return;
    }

    k_work_schedule(&blink, K_NO_WAIT);
}

static int set_new_led_state(enum led_state new_state) {
    int ret = 0;

    k_mutex_lock(&led_mtx, K_FOREVER);

    if (new_state == state) {
        ret = -EAGAIN;
    } else {
        state = new_state;
    }

    k_mutex_unlock(&led_mtx);

    if (ret < 0) {
        return ret;
    }

    switch (new_state) {
    case OFF: {
        return gpio_pin_set_dt(&led, LED_OFF);
    }
    case ON: {
        return gpio_pin_set_dt(&led, LED_ON);
    }
    case BLINK: {
        ret = k_work_schedule(&blink, K_NO_WAIT);
        break;
    }
    default: {
        ret = -EINVAL;
        break;
    }
    }

    if (ret >= 0) {
        return 0;
    }

    return ret;
}

int update_led_state(uint8_t new_state) {
    if (new_state >= LED_INVALID) {
        return state;
    }

    return set_new_led_state(new_state);
}

int main(void) {
    start_led();

    return 0;
}
