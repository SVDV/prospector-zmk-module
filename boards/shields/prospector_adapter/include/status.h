#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/sys/util.h>
#include <zmk/endpoints_types.h>

/*
 * One snapshot of everything the views show, collected from ZMK events.
 * Subscribers are always called on the display work queue, so they may use LVGL.
 */

#define PSPTR_MAX_PERIPHERALS 3

enum psptr_change {
    PSPTR_CHANGED_LAYER = BIT(0),
    PSPTR_CHANGED_MODS = BIT(1),
    PSPTR_CHANGED_PERIPHERALS = BIT(2),
    PSPTR_CHANGED_ENDPOINT = BIT(3),
    PSPTR_CHANGED_WPM = BIT(4),
    PSPTR_CHANGED_ALL = 0xFF,
};

struct psptr_peripheral {
    uint8_t level;
    bool connected;
};

struct psptr_status {
    uint8_t layer; /* highest active layer index */
    bool ctrl, alt, gui, shift;
    bool caps_word;
    uint8_t peripheral_count;
    struct psptr_peripheral peripherals[PSPTR_MAX_PERIPHERALS];
    struct zmk_endpoint_instance endpoint;
    int wpm; /* -1 when CONFIG_ZMK_WPM is off */
};

typedef void (*psptr_status_cb)(const struct psptr_status *status, uint32_t changed);

/* Register before psptr_status_start(). */
void psptr_status_subscribe(psptr_status_cb cb);

/* Read the initial state and deliver it to every subscriber. Call on the display thread. */
void psptr_status_start(void);
