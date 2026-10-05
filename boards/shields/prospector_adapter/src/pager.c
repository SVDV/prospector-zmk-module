#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zmk/display.h>

#include <pager.h>
#include <theme.h>
#include <ui.h>

#define MAX_PAGES 4
#define SLIDE_MS 250
#define DOTS_HOLD_MS 900
#define DOTS_FADE_MS 300
#define DOT 6
#define DOT_PITCH 12

static const bool vertical = IS_ENABLED(CONFIG_PROSPECTOR_SWIPE_VIEWS_VERTICAL);

static lv_obj_t *screen;
static lv_obj_t *pages[MAX_PAGES];
static size_t page_count;
static size_t current;
static lv_obj_t *sliding_out;
static lv_obj_t *dots;
static lv_obj_t *dot[MAX_PAGES];

void psptr_pager_init(lv_obj_t *scr) { screen = scr; }

lv_obj_t *psptr_pager_add_page(void) {
    if (page_count == MAX_PAGES) {
        return NULL;
    }
    lv_obj_t *page = psptr_obj(screen, 0, 0, PSPTR_W, PSPTR_H);
    lv_obj_set_style_bg_color(page, psptr_theme()->bg, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    pages[page_count++] = page;
    return page;
}

static void set_offset(void *obj, int32_t v) {
    if (vertical) {
        lv_obj_set_y(obj, v);
    } else {
        lv_obj_set_x(obj, v);
    }
}

/* fade each dot's own background: an opa style on the container would force a layer */
static void set_opa(void *obj, int32_t v) {
    uint32_t n = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_set_style_bg_opa(lv_obj_get_child(obj, i), (lv_opa_t)v, 0);
    }
}

static void show_dots(void) {
    if (!dots) {
        return;
    }
    const struct psptr_theme *t = psptr_theme();
    for (size_t i = 0; i < page_count; i++) {
        lv_obj_set_style_bg_color(dot[i], i == current ? t->text : t->faint, 0);
    }
    lv_anim_delete(dots, set_opa);
    set_opa(dots, LV_OPA_COVER);
    psptr_show(dots, true);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, dots);
    lv_anim_set_exec_cb(&a, set_opa);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_delay(&a, DOTS_HOLD_MS);
    lv_anim_set_duration(&a, DOTS_FADE_MS);
    lv_anim_start(&a);
}

void psptr_pager_finish(void) {
    if (page_count == 0) {
        return;
    }
    current = 0;
    psptr_show(pages[0], true);
    if (page_count < 2) {
        return;
    }

    /* dots: centred along the bottom edge, or the right edge for vertical paging */
    int32_t span = (int32_t)(page_count - 1) * DOT_PITCH + DOT;
    dots = vertical ? psptr_obj(screen, 269, (PSPTR_H - span) / 2, DOT, span)
                    : psptr_obj(screen, (PSPTR_W - span) / 2, 227, span, DOT);
    for (size_t i = 0; i < page_count; i++) {
        int32_t o = (int32_t)i * DOT_PITCH;
        dot[i] = psptr_rect(dots, vertical ? 0 : o, vertical ? o : 0, DOT, DOT, LV_RADIUS_CIRCLE,
                            psptr_theme()->faint);
    }
    psptr_show(dots, false);
}

static void slide_done(lv_anim_t *a) {
    ARG_UNUSED(a);
    if (sliding_out) {
        psptr_show(sliding_out, false);
        set_offset(sliding_out, 0);
        sliding_out = NULL;
    }
}

static void start_slide(lv_obj_t *obj, int32_t from, int32_t to, bool last) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, set_offset);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, SLIDE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    if (last) {
        lv_anim_set_completed_cb(&a, slide_done);
    }
    lv_anim_start(&a);
}

void psptr_pager_step(int dir) {
    if (page_count < 2 || sliding_out) {
        return; /* ignore swipes while a slide is running */
    }
    size_t next = (current + page_count + (dir > 0 ? 1 : page_count - 1)) % page_count;
    int32_t span = vertical ? PSPTR_H : PSPTR_W;
    int32_t sign = dir > 0 ? 1 : -1;

    sliding_out = pages[current];
    lv_obj_t *in = pages[next];
    set_offset(in, sign * span);
    psptr_show(in, true);
    current = next;

    start_slide(sliding_out, 0, -sign * span, false);
    start_slide(in, sign * span, 0, true);
    show_dots();
}

/* ---------- cross-thread entry ---------- */

static atomic_t posted_dir;

static void step_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    int dir = (int)atomic_set(&posted_dir, 0);
    if (dir) {
        psptr_pager_step(dir);
    }
}

static K_WORK_DEFINE(step_work, step_work_cb);

void psptr_pager_post_step(int dir) {
    if (!zmk_display_is_initialized()) {
        return;
    }
    atomic_set(&posted_dir, dir);
    k_work_submit_to_queue(zmk_display_work_q(), &step_work);
}
