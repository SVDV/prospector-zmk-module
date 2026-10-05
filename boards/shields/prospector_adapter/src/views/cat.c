/*
 * View 4: the kaomoji cat. It faces the half of the keyboard you last pressed a key on, with the
 * active layer's name above it and the battery of each half below.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

#include <fonts.h>
#include <layout.h>
#include <status.h>
#include <theme.h>
#include <ui.h>
#include <views.h>

LV_IMAGE_DECLARE(psptr_cat_left);
LV_IMAGE_DECLARE(psptr_cat_right);

#define TITLE_Y 12
#define CAT_TOP 52    /* space between the title and the battery row */
#define CAT_BOTTOM 198

static lv_obj_t *title;
static lv_obj_t *cat;
static struct psptr_battery_pair batteries;
static bool facing_left = true; /* the art faces left as drawn */
static bool started;

/* Last pressed key position + 1 (0 = none yet). Written by the listener, read on the display thread. */
static atomic_t last_press;

static void face_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    atomic_val_t v = atomic_get(&last_press);
    if (v == 0) {
        return;
    }
    bool left = psptr_layout_is_left((uint32_t)(v - 1));
    if (left != facing_left) {
        facing_left = left;
        lv_image_set_src(cat, left ? &psptr_cat_left : &psptr_cat_right);
    }
}

static K_WORK_DEFINE(face_work, face_work_cb);

static int on_position(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev && ev->state) {
        atomic_set(&last_press, (atomic_val_t)ev->position + 1);
        if (started) {
            k_work_submit_to_queue(zmk_display_work_q(), &face_work);
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(psptr_cat, on_position);
ZMK_SUBSCRIPTION(psptr_cat, zmk_position_state_changed);

static void update(const struct psptr_status *s, uint32_t changed) {
    if (changed & PSPTR_CHANGED_LAYER) {
        const char *name = psptr_layer_name(s->layer);
        lv_label_set_text(title, name);
        lv_obj_set_x(title, (PSPTR_W - psptr_text_width(&FR_Regular_30, name)) / 2);
        lv_obj_set_style_text_color(title, psptr_layer_color(s->layer), 0);
    }
    if (changed & PSPTR_CHANGED_PERIPHERALS) {
        psptr_battery_pair_set(&batteries, s);
    }
}

void psptr_view_cat_create(lv_obj_t *page) {
    const struct psptr_theme *t = psptr_theme();

    title = psptr_label(page, &FR_Regular_30, t->text);
    lv_obj_set_y(title, TITLE_Y);

    /* an A8 image is drawn as a mask filled with the recolour colour */
    cat = lv_image_create(page);
    lv_obj_remove_flag(cat, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(cat, &psptr_cat_left);
    lv_obj_set_style_image_recolor(cat, t->cat, 0);
    lv_obj_set_style_image_recolor_opa(cat, LV_OPA_COVER, 0);
    lv_obj_set_pos(cat, (PSPTR_W - psptr_cat_left.header.w) / 2,
                   CAT_TOP + (CAT_BOTTOM - CAT_TOP - psptr_cat_left.header.h) / 2);

    psptr_battery_pair_create(&batteries, page);
    psptr_status_subscribe(update);
    started = true;
}
