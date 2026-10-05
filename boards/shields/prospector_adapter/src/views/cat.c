/*
 * View 4: the kaomoji cat. It turns its head towards the half of the keyboard you last pressed a
 * key on, with the active layer's name above it and the battery of each half below.
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

LV_IMAGE_DECLARE(psptr_cat_body);
extern const lv_image_dsc_t *const psptr_cat_heads[5]; /* facing left ... facing right */

#define HEAD_FACING_LEFT 0
#define HEAD_FACING_RIGHT 4
#define TURN_FRAME_MS 30 /* four steps: a ~120 ms turn */

#define TITLE_Y 12
#define CAT_TOP 52    /* space between the title and the battery row */
#define CAT_BOTTOM 198

static lv_obj_t *title;
static lv_obj_t *head;
static lv_timer_t *turn_timer;
static struct psptr_battery_pair batteries;
static int head_frame = HEAD_FACING_LEFT; /* the art faces left as drawn */
static int head_target = HEAD_FACING_LEFT;
static bool started;

/* Last pressed key position + 1 (0 = none yet). Written by the listener, read on the display thread. */
static atomic_t last_press;

static void face_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    atomic_val_t v = atomic_get(&last_press);
    if (v == 0) {
        return;
    }
    head_target = psptr_layout_is_left((uint32_t)(v - 1)) ? HEAD_FACING_LEFT : HEAD_FACING_RIGHT;
    if (head_target != head_frame) {
        lv_timer_resume(turn_timer); /* a turn already under way just carries on towards the new side */
    }
}

/* One frame of the head turn per tick: squash to a sliver around its centre, widen mirrored. */
static void turn_tick(lv_timer_t *timer) {
    head_frame += head_target > head_frame ? 1 : -1;
    lv_image_set_src(head, psptr_cat_heads[head_frame]);
    if (head_frame == head_target) {
        lv_timer_pause(timer);
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

/* An A8 image is drawn as a mask filled with the recolour colour. */
static lv_obj_t *cat_image(lv_obj_t *page, const struct psptr_theme *t,
                           const lv_image_dsc_t *src) {
    lv_obj_t *img = lv_image_create(page);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(img, src);
    lv_obj_set_style_image_recolor(img, t->cat, 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    return img;
}

void psptr_view_cat_create(lv_obj_t *page) {
    const struct psptr_theme *t = psptr_theme();
    psptr_layout_init();

    title = psptr_label(page, &FR_Regular_30, t->text);
    lv_obj_set_y(title, TITLE_Y);

    /* the head sits directly on the body; together they're centred between title and batteries */
    const lv_image_dsc_t *head_src = psptr_cat_heads[HEAD_FACING_LEFT];
    int32_t x = (PSPTR_W - psptr_cat_body.header.w) / 2;
    int32_t y = CAT_TOP + (CAT_BOTTOM - CAT_TOP - head_src->header.h - psptr_cat_body.header.h) / 2;
    lv_obj_t *body = cat_image(page, t, &psptr_cat_body);
    lv_obj_set_pos(body, x, y + head_src->header.h);
    head = cat_image(page, t, head_src);
    lv_obj_set_pos(head, x, y);

    turn_timer = lv_timer_create(turn_tick, TURN_FRAME_MS, NULL);
    lv_timer_pause(turn_timer);

    psptr_battery_pair_create(&batteries, page);
    psptr_status_subscribe(update);
    started = true;
}
