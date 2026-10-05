#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>

#if IS_ENABLED(CONFIG_ZMK_WPM)
#include <zmk/events/wpm_state_changed.h>
#include <zmk/wpm.h>
#endif

#if IS_ENABLED(CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED)
#include <zmk/events/caps_word_state_changed.h>
#endif

#include <status.h>

#define MAX_SUBSCRIBERS 4
#define PERIPHERALS MIN(ZMK_SPLIT_BLE_PERIPHERAL_COUNT, PSPTR_MAX_PERIPHERALS)

static psptr_status_cb subscribers[MAX_SUBSCRIBERS];
static size_t subscriber_count;

/* Written by event listeners (any thread), read by the display work item. */
static struct psptr_status pending;
static uint8_t held_mods; /* HID modifier bits 0xE0..0xE7, from keycode events */
static K_MUTEX_DEFINE(pending_lock);
static atomic_t pending_changes;
static bool started;

static void deliver(struct k_work *work) {
    ARG_UNUSED(work);
    uint32_t changed = (uint32_t)atomic_clear(&pending_changes);
    if (!changed) {
        return;
    }

    k_mutex_lock(&pending_lock, K_FOREVER);
    struct psptr_status copy = pending;
    k_mutex_unlock(&pending_lock);

    for (size_t i = 0; i < subscriber_count; i++) {
        subscribers[i](&copy, changed);
    }
}

static K_WORK_DEFINE(deliver_work, deliver);

static void mark_changed(uint32_t what) {
    atomic_or(&pending_changes, what);
    if (started && zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &deliver_work);
    }
}

static void apply_mods_locked(void) {
    pending.ctrl = held_mods & (BIT(0) | BIT(4));
    pending.shift = held_mods & (BIT(1) | BIT(5));
    pending.alt = held_mods & (BIT(2) | BIT(6));
    pending.gui = held_mods & (BIT(3) | BIT(7));
}

static int on_event(const zmk_event_t *eh) {
    uint32_t what = 0;
    k_mutex_lock(&pending_lock, K_FOREVER);

    if (as_zmk_layer_state_changed(eh)) {
        pending.layer = zmk_keymap_highest_layer_active();
        what = PSPTR_CHANGED_LAYER;
    } else if (as_zmk_keycode_state_changed(eh)) {
        const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
        if (ev->usage_page == HID_USAGE_KEY && ev->keycode >= HID_USAGE_KEY_KEYBOARD_LEFTCONTROL &&
            ev->keycode <= HID_USAGE_KEY_KEYBOARD_RIGHT_GUI) {
            uint8_t bit = BIT(ev->keycode - HID_USAGE_KEY_KEYBOARD_LEFTCONTROL);
            held_mods = ev->state ? (held_mods | bit) : (held_mods & ~bit);
            apply_mods_locked();
            what = PSPTR_CHANGED_MODS;
        }
    } else if (as_zmk_peripheral_battery_state_changed(eh)) {
        const struct zmk_peripheral_battery_state_changed *ev =
            as_zmk_peripheral_battery_state_changed(eh);
        if (ev->source < PERIPHERALS) {
            pending.peripherals[ev->source].level = ev->state_of_charge;
            what = PSPTR_CHANGED_PERIPHERALS;
        }
    } else if (as_zmk_split_central_status_changed(eh)) {
        const struct zmk_split_central_status_changed *ev = as_zmk_split_central_status_changed(eh);
        if (ev->slot < PERIPHERALS) {
            pending.peripherals[ev->slot].connected = ev->connected;
            what = PSPTR_CHANGED_PERIPHERALS;
        }
    } else if (as_zmk_endpoint_changed(eh)) {
        pending.endpoint = as_zmk_endpoint_changed(eh)->endpoint;
        what = PSPTR_CHANGED_ENDPOINT;
    }
#if IS_ENABLED(CONFIG_ZMK_WPM)
    else if (as_zmk_wpm_state_changed(eh)) {
        pending.wpm = as_zmk_wpm_state_changed(eh)->state;
        what = PSPTR_CHANGED_WPM;
    }
#endif
#if IS_ENABLED(CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED)
    else if (as_zmk_caps_word_state_changed(eh)) {
        pending.caps_word = as_zmk_caps_word_state_changed(eh)->active;
        what = PSPTR_CHANGED_MODS;
    }
#endif

    k_mutex_unlock(&pending_lock);
    if (what) {
        mark_changed(what);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(psptr_status, on_event);
ZMK_SUBSCRIPTION(psptr_status, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(psptr_status, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(psptr_status, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(psptr_status, zmk_split_central_status_changed);
ZMK_SUBSCRIPTION(psptr_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_ZMK_WPM)
ZMK_SUBSCRIPTION(psptr_status, zmk_wpm_state_changed);
#endif
#if IS_ENABLED(CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED)
ZMK_SUBSCRIPTION(psptr_status, zmk_caps_word_state_changed);
#endif

void psptr_status_subscribe(psptr_status_cb cb) {
    if (subscriber_count < MAX_SUBSCRIBERS) {
        subscribers[subscriber_count++] = cb;
    }
}

void psptr_status_start(void) {
    k_mutex_lock(&pending_lock, K_FOREVER);
    pending.layer = zmk_keymap_highest_layer_active();
    held_mods = zmk_hid_get_explicit_mods();
    apply_mods_locked();
    pending.peripheral_count = PERIPHERALS;
    pending.endpoint = zmk_endpoint_get_selected();
#if IS_ENABLED(CONFIG_ZMK_WPM)
    pending.wpm = zmk_wpm_get_state();
#else
    pending.wpm = -1;
#endif
    k_mutex_unlock(&pending_lock);

    started = true;
    atomic_or(&pending_changes, PSPTR_CHANGED_ALL);
    deliver(NULL);
}
