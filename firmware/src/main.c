#include <led.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define SLEEP_TIME_MS 1000
#define TRY_AGAIN_MS  10

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

// clang-format off
static struct pwm_led pwm_leds[] = { // NOLINT(*-interfaces-global-init)
    // clang-format on
    {
        .led = PWM_DT_SPEC_GET(DT_NODELABEL(pwm_led0)),
        .internal_state = INTERNAL_BLINK_OFF,
        .state = LED_STATE_BLINK,
    },

    {
        .led = PWM_DT_SPEC_GET(DT_NODELABEL(pwm_led1)),
        .internal_state = INTERNAL_OFF,
        .state = LED_STATE_OFF,
    },
};

struct pwm_led *pwm_led_from_blink_work(struct k_work_delayable *dwork) {
    return CONTAINER_OF(dwork, struct pwm_led, blink_work);
}

struct pwm_led *pwm_led_from_fade_work(struct k_work_delayable *dwork) {
    return CONTAINER_OF(dwork, struct pwm_led, fade_work);
}

static void fade_up(struct pwm_led *led) {
    led->next_width += led->step;
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

    if (led->state != LED_STATE_FADE) {
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
    enum led_state_internal new_state;
    uint32_t period;

    int ret = k_mutex_lock(&led->lock, K_NO_WAIT);
    if (ret < 0) {
        k_work_schedule(dwork, K_MSEC(TRY_AGAIN_MS));
        return;
    }

    if (led->state != LED_STATE_BLINK) {
        k_mutex_unlock(&led->lock);
        return;
    }

    switch (led->internal_state) {
    case INTERNAL_BLINK_ON: {
        period = 0u;
        new_state = INTERNAL_BLINK_OFF;
        break;
    }
    case INTERNAL_BLINK_OFF: {
        period = led->led.period;
        new_state = INTERNAL_BLINK_ON;
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
    } else {
        led->internal_state = new_state;
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

static int update_led_blink_locked(struct pwm_led *led) {
    int ret = 0;

    switch (led->internal_state) {
    case INTERNAL_OFF:
    case INTERNAL_FADE_UP: {
        led->internal_state = INTERNAL_BLINK_ON;
        ret = pwm_set_pulse_dt(&led->led, led->led.period);
        break;
    }
    case INTERNAL_ON:
    case INTERNAL_FADE_DOWN: {
        led->internal_state = INTERNAL_BLINK_OFF;
        ret = pwm_set_pulse_dt(&led->led, 0u);
        break;
    }
    default: {
        ret = -EALREADY;
        break;
    }
    }

    if (ret < 0 && k_work_delayable_busy_get(&led->blink_work) != 0) {
        return ret;
    }

    return k_work_schedule(&led->blink_work, K_NO_WAIT);
}

static int update_led_fade_locked(struct pwm_led *led) {
    int ret = 0;

    switch (led->internal_state) {
    case INTERNAL_OFF:
    case INTERNAL_BLINK_OFF: {
        led->internal_state = INTERNAL_FADE_UP;
        ret = pwm_set_pulse_dt(&led->led, 0u);
        break;
    }
    case INTERNAL_ON:
    case INTERNAL_BLINK_ON: {
        led->internal_state = INTERNAL_FADE_DOWN;
        ret = pwm_set_pulse_dt(&led->led, led->led.period);
        break;
    }
    default: {
        ret = -EALREADY;
        break;
    }
    }

    if (ret < 0 && k_work_delayable_busy_get(&led->fade_work) != 0) {
        return ret;
    }

    return k_work_schedule(&led->fade_work, K_NO_WAIT);
}

static int update_led_state_locked(struct pwm_led *led,
                                   enum led_state new_state) {
    int ret;

    switch (new_state) {
    case LED_STATE_ON: {
        ret = pwm_set_pulse_dt(&led->led, led->led.period);
        break;
    }
    case LED_STATE_OFF: {
        ret = pwm_set_pulse_dt(&led->led, 0u);
        break;
    }
    case LED_STATE_BLINK: {
        ret = update_led_blink_locked(led);
        break;
    }
    case LED_STATE_FADE: {
        ret = update_led_fade_locked(led);
        break;
    }
    default: {
        ret = -EINVAL;
        break;
    }
    }

    if (ret > 0) {
        return 0;
    }

    return ret;
}

static void start_leds(void) {
    init_leds();

    for (size_t i = 0u; i < ARRAY_SIZE(pwm_leds); i++) {
        k_mutex_lock(&pwm_leds[i].lock, K_FOREVER);
        (void)update_led_state_locked(&pwm_leds[i], pwm_leds[i].state);
        k_mutex_unlock(&pwm_leds[i].lock);
    }
}

static int update_led_state_(enum led led_, uint8_t new_state) {
    int ret;
    struct pwm_led *led = &pwm_leds[led_];

    k_mutex_lock(&led->lock, K_FOREVER);
    if (new_state == led->state) {
        k_mutex_unlock(&led->lock);
        return -EALREADY;
    }

    led->state = new_state;
    ret = update_led_state_locked(led, new_state);
    k_mutex_unlock(&led->lock);

    return ret;
}

int update_led_state(enum led led, uint8_t new_state) {
    if (led >= LED_INVALID) {
        return -EINVAL;
    }

    if (new_state >= LED_STATE_INVALID) {
        return pwm_leds[led].state;
    }

    return update_led_state_(led, new_state);
}

int main(void) {
    start_leds();

    return 0;
}
