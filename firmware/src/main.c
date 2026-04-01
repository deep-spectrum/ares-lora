#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define LED0_NODE     DT_ALIAS(led0)
#define SLEEP_TIME_MS 1000

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static bool led_active = true;

static void blink_work(struct k_work *work) {
    struct k_work_delayable *dwork =
        CONTAINER_OF(work, struct k_work_delayable, work);

    if (!led_active) {
        (void)gpio_pin_set_dt(&led, 0);
        return;
    }

    (void)gpio_pin_toggle_dt(&led);
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

int main(void) {
    start_led();

    return 0;
}
