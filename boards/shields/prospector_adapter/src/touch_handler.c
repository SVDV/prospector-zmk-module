/*
 * Touch handler for the CST816S touch panel: swipes switch views and change brightness.
 *
 * The CST816S reports gestures in the panel's own portrait orientation, but the display is
 * rotated 270° (or 90° with CONFIG_PROSPECTOR_ROTATE_DISPLAY_180). With the default rotation a
 * finger moving right on the landscape screen arrives as SWIPE_UP, left as SWIPE_DOWN, up as
 * SWIPE_RIGHT and down as SWIPE_LEFT (verified on the device).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <pager.h>

LOG_MODULE_REGISTER(touch_handler, CONFIG_LOG_DEFAULT_LEVEL);

/* External brightness API from brightness.c */
extern uint8_t psptr_get_display_brightness(void);
extern int psptr_set_display_brightness(uint8_t brightness);

/* Cooldown between swipe actions to prevent rapid-fire adjustments */
#define SWIPE_COOLDOWN_MS 400

/* CST816S gesture codes (panel orientation) */
#define GESTURE_SWIPE_UP 0x01
#define GESTURE_SWIPE_DOWN 0x02
#define GESTURE_SWIPE_LEFT 0x03
#define GESTURE_SWIPE_RIGHT 0x04

enum finger { FINGER_NONE, FINGER_LEFT, FINGER_RIGHT, FINGER_UP, FINGER_DOWN };

static int64_t last_swipe_time;

static enum finger finger_direction(uint16_t code) {
    enum finger f;
    switch (code) {
    case GESTURE_SWIPE_UP:
        f = FINGER_RIGHT;
        break;
    case GESTURE_SWIPE_DOWN:
        f = FINGER_LEFT;
        break;
    case GESTURE_SWIPE_LEFT:
        f = FINGER_DOWN;
        break;
    case GESTURE_SWIPE_RIGHT:
        f = FINGER_UP;
        break;
    default:
        return FINGER_NONE;
    }
#if IS_ENABLED(CONFIG_PROSPECTOR_ROTATE_DISPLAY_180)
    static const enum finger flipped[] = {FINGER_NONE, FINGER_RIGHT, FINGER_LEFT, FINGER_DOWN,
                                          FINGER_UP};
    f = flipped[f];
#endif
    return f;
}

static void change_brightness(bool increase) {
    uint8_t current = psptr_get_display_brightness();
    uint8_t step = CONFIG_PROSPECTOR_BRIGHTNESS_STEP;
    uint8_t next = increase ? MIN(current + step, 100) : (current <= step ? 1 : current - step);

    if (next != current) {
        psptr_set_display_brightness(next);
        LOG_INF("Touch swipe: brightness %d -> %d", current, next);
    }
}

static void handle_swipe(enum finger f) {
    int64_t now = k_uptime_get();
    if ((now - last_swipe_time) < SWIPE_COOLDOWN_MS) {
        return;
    }
    last_swipe_time = now;

#if IS_ENABLED(CONFIG_PROSPECTOR_SWIPE_VIEWS_VERTICAL)
    switch (f) {
    case FINGER_UP:
        psptr_pager_post_step(1);
        break;
    case FINGER_DOWN:
        psptr_pager_post_step(-1);
        break;
    case FINGER_RIGHT:
        change_brightness(true);
        break;
    case FINGER_LEFT:
        change_brightness(false);
        break;
    default:
        break;
    }
#else
    switch (f) {
    case FINGER_LEFT:
        psptr_pager_post_step(1);
        break;
    case FINGER_RIGHT:
        psptr_pager_post_step(-1);
        break;
    case FINGER_UP:
        change_brightness(true);
        break;
    case FINGER_DOWN:
        change_brightness(false);
        break;
    default:
        break;
    }
#endif
}

static void touch_input_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);

    /* Gesture events from the CST816S are reported as INPUT_EV_DEVICE */
    if (evt->type == INPUT_EV_DEVICE) {
        enum finger f = finger_direction(evt->code);
        if (f != FINGER_NONE) {
            handle_swipe(f);
        }
    }
}

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
