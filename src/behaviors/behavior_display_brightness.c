/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_display_brightness

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>

#include <dt-bindings/zmk/display_brightness.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

/* External functions from brightness.c */
extern uint8_t psptr_get_display_brightness(void);
extern int psptr_set_display_brightness(uint8_t brightness);
extern uint8_t psptr_get_last_display_brightness(void);

static int on_display_brightness_binding_pressed(struct zmk_behavior_binding *binding,
                                                  struct zmk_behavior_binding_event event) {
    uint8_t current = psptr_get_display_brightness();
    uint8_t step = CONFIG_PROSPECTOR_BRIGHTNESS_STEP;
    uint8_t new_brightness;

    switch (binding->param1) {
    case DISP_BRI_TOG_CMD:
        if (current > 0) {
            return psptr_set_display_brightness(0);
        } else {
            return psptr_set_display_brightness(psptr_get_last_display_brightness());
        }

    case DISP_BRI_INC_CMD:
        new_brightness = current + step;
        if (new_brightness > 100) {
            new_brightness = 100;
        }
        return psptr_set_display_brightness(new_brightness);

    case DISP_BRI_DEC_CMD:
        if (current <= step) {
            new_brightness = 1;
        } else {
            new_brightness = current - step;
        }
        return psptr_set_display_brightness(new_brightness);

    case DISP_BRI_SET_CMD:
        return psptr_set_display_brightness(binding->param2);

    default:
        LOG_ERR("Unknown display brightness command: %d", binding->param1);
        return -ENOTSUP;
    }
}

static int on_display_brightness_binding_released(struct zmk_behavior_binding *binding,
                                                   struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_display_brightness_driver_api = {
    .binding_pressed = on_display_brightness_binding_pressed,
    .binding_released = on_display_brightness_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

static int behavior_display_brightness_init(const struct device *dev) {
    return 0;
}

#define DISP_BRI_INST(n)                                                                           \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_display_brightness_init, NULL, NULL, NULL, POST_KERNEL,    \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_display_brightness_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DISP_BRI_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
