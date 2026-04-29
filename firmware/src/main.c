#include <led.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define LED0_NODE     DT_ALIAS(led0)
#define SLEEP_TIME_MS 1000
#define TRY_AGAIN_MS  10
#define LED_ON        0
#define LED_OFF       1

#define PWM_SLEEP_MS  25u
#define PWM_STEPS     50u

enum led_state_internal {
    INTERNAL_OFF,
    INTERNAL_ON,
    INTERNAL_BLINK_OFF,
    INTERNAL_BLINK_ON,
    INTERNAL_FADE_UP,
    INTERNAL_FADE_DOWN,
};

struct pwm_led {
    const struct pwm_dt_spec led;
    uint32_t next_width;
    uint32_t step;
    enum led_state_internal internal_state;
    enum led_state state;
    struct k_mutex lock;
    struct k_work_delayable blink_work;
    struct k_work_delayable fade_work;
};

static struct pwm_led pwm_leds[] = {
    {
        .led = PWM_DT_SPEC_GET(DT_NODELABEL(pwm_led0)),
        .internal_state = INTERNAL_BLINK_OFF,
        .state = BLINK,
    },
};

struct pwm_led *pwm_led_from_blink_work(struct k_work_delayable *dwork) {
    return CONTAINER_OF(dwork, struct pwm_led, blink_work);
}

struct pwm_led *pwm_led_from_fade_work(struct k_work_delayable *dwork) {
    return CONTAINER_OF(dwork, struct pwm_led, fade_work);
}

static void fade_up(struct pwm_led *led) {
    led->next_width += led->led.period;
    if (led->next_width >= led->led.period) {
        led->next_width = led->led.period;
        led->internal_state = INTERNAL_FADE_DOWN;
    }
}

static void fade_down(struct pwm_led *led) {
    if (led->next_width <= led->step) {
        led->next_width = 0;
        led->internal_state = INTERNAL_FADE_UP;
    } else {
        led->next_width -= led->step;
    }
}

static void fade_work(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct pwm_led *led = pwm_led_from_fade_work(dwork);

    int ret = k_mutex_lock(&led->lock, K_NO_WAIT);
    if (ret < 0) {
        k_work_schedule(dwork, K_MSEC(TRY_AGAIN_MS));
        return;
    }

    if (led->state != FADE) {
        k_mutex_unlock(&led->lock);
        return;
    }

    ret = pwm_set_pulse_dt(&led->led, led->next_width);
    if (ret < 0) {
        LOG_ERR("pwm_set_pulse_dt(): %d", ret);
    }

    switch (led->internal_state) {
    case INTERNAL_FADE_UP: {
        fade_up(led);
        break;
    }
    case INTERNAL_FADE_DOWN: {
        fade_down(led);
        break;
    }
    default: {
        __ASSERT(false, "Invalid internal state: %d", led->internal_state);
        break;
    }
    }

    k_mutex_unlock(&led->lock);
    k_work_schedule(dwork, K_MSEC(PWM_SLEEP_MS));
}

static void blink_work(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct pwm_led *led = pwm_led_from_blink_work(dwork);
    uint32_t period;

    int ret = k_mutex_lock(&led->lock, K_NO_WAIT);
    if (ret < 0) {
        k_work_schedule(dwork, K_MSEC(TRY_AGAIN_MS));
        return;
    }

    if (led->state != BLINK) {
        k_mutex_unlock(&led->lock);
        return;
    }

    switch (led->internal_state) {
    case INTERNAL_BLINK_ON: {
        period = 0u;
        break;
    }
    case INTERNAL_BLINK_OFF: {
        period = led->led.period;
        break;
    }
    default: {
        __ASSERT(false, "Invalid internal state: %d", led->internal_state);
        k_mutex_unlock(&led->lock);
        return;
    }
    }

    ret = pwm_set_pulse_dt(&led->led, period);
    if (ret < 0) {
        LOG_ERR("pwm_set_pulse_dt(): %d", ret);
    }

    k_mutex_unlock(&led->lock);
    k_work_schedule(dwork, K_MSEC(SLEEP_TIME_MS));
}

static void init_leds(void) {
    for (size_t i = 0u; i < ARRAY_SIZE(pwm_leds); i++) {
        pwm_leds[i].next_width = 0;
        pwm_leds[i].step = pwm_leds[i].led.period / PWM_STEPS;
        k_mutex_init(&pwm_leds[i].lock);
        k_work_init_delayable(&pwm_leds[i].blink_work, blink_work);
        k_work_init_delayable(&pwm_leds[i].fade_work, fade_work);
        if (!pwm_is_ready_dt(&pwm_leds[i].led)) {
            LOG_ERR("PWM device not ready: %s", pwm_leds[i].led.dev->name);
            return;
        }
    }
}

// static int z_update_led_blink(struct pwm_led *led) {
//     int ret = 0;
//
// }
//
// static int z_update_led_state(struct pwm_led *led, enum led_state new_state)
// {
//     int ret;
//
//     switch (new_state) {
//         case ON: {
//             ret = pwm_set_pulse_dt(&led->led, led->led.period);
//             break;
//         }
//         case OFF: {
//             ret = pwm_set_pulse_dt(&led->led, 0u);
//             break;
//         }
//         case BLINK:
//     }
// }

static void start_led(void) {
    int ret;

    // if (!gpio_is_ready_dt(&led)) {
    //     LOG_ERR("LED GPIO not ready");
    //     return;
    // }

    // ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
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
        // return gpio_pin_set_dt(&led, LED_OFF);
    }
    case ON: {
        // return gpio_pin_set_dt(&led, LED_ON);
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
