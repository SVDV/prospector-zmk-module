/*
 * Touch handler for CST816S touch panel
 * Handles swipe gestures to control display brightness
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(touch_handler, CONFIG_LOG_DEFAULT_LEVEL);

/* External brightness API from brightness.c */
extern uint8_t psptr_get_display_brightness(void);
extern int psptr_set_display_brightness(uint8_t brightness);

/* Cooldown between swipe actions to prevent rapid-fire adjustments */
#define SWIPE_COOLDOWN_MS 400

/* Track last swipe time for cooldown */
static int64_t last_swipe_time;

/* CST816S gesture codes */
#define GESTURE_SWIPE_UP    0x01
#define GESTURE_SWIPE_DOWN  0x02
#define GESTURE_SWIPE_LEFT  0x03
#define GESTURE_SWIPE_RIGHT 0x04

static void handle_brightness_swipe(bool increase) {
    int64_t now = k_uptime_get();

    /* Check cooldown */
    if ((now - last_swipe_time) < SWIPE_COOLDOWN_MS) {
        return;
    }
    last_swipe_time = now;

    uint8_t current = psptr_get_display_brightness();
    uint8_t step = CONFIG_PROSPECTOR_BRIGHTNESS_STEP;
    uint8_t new_brightness;

    if (increase) {
        new_brightness = (current + step > 100) ? 100 : current + step;
    } else {
        new_brightness = (current <= step) ? 1 : current - step;
    }

    if (new_brightness != current) {
        psptr_set_display_brightness(new_brightness);
        LOG_INF("Touch swipe: brightness %d -> %d", current, new_brightness);
    }
}

static void touch_input_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);

    /* Handle gesture events from CST816S */
    if (evt->code == INPUT_ABS_MISC) {
        switch (evt->value) {
        case GESTURE_SWIPE_UP:
            handle_brightness_swipe(true);  /* Swipe up = increase brightness */
            break;
        case GESTURE_SWIPE_DOWN:
            handle_brightness_swipe(false); /* Swipe down = decrease brightness */
            break;
        default:
            /* Ignore other gestures */
            break;
        }
    }
}

/* Register input callback for the touch sensor */
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET_OR_NULL(DT_NODELABEL(touch_sensor)), touch_input_cb, NULL);

static int touch_handler_init(void) {
    const struct device *touch_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(touch_sensor));

    if (touch_dev == NULL) {
        LOG_WRN("Touch sensor not found in device tree");
        return -ENODEV;
    }

    if (!device_is_ready(touch_dev)) {
        LOG_WRN("Touch sensor device not ready");
        return -ENODEV;
    }

    LOG_INF("Touch handler initialized: CST816S on I2C");
    return 0;
}

SYS_INIT(touch_handler_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
