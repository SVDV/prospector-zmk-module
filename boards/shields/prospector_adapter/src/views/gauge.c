/*
 * View 3: WPM gauge.
 * A 270° dial whose ticks light in the layer colour, the WPM in PP Formula Narrow Thin, the
 * layer as a pill, mods in the gap under the dial and a fuel gauge per half on each side.
 * Needs CONFIG_ZMK_WPM (the central computes it; the halves don't change).
 */

#include <math.h>
#include <string.h>

#include <fonts.h>
#include <status.h>
#include <theme.h>
#include <ui.h>
#include <views.h>

#define CX 140
#define CY 126
#define RADIUS 92
#define ARC_W 10
#define ARC_START 135
#define ARC_SWEEP 270
#define TICKS 30
#define WPM_MAX 150
#define PILL_Y 158
#define PILL_H 24
#define MODS_Y 199
#define MODS_GAP 4
#define GAUGE_Y 68
#define GAUGE_H 104
#define PI_F 3.14159265f
#define GLIDE_MS 400   /* arc and number glide to each new WPM, like the battery bars */
#define RING_IN (RADIUS - ARC_W)
#define RING_OUT 103   /* outer end of the ticks */
#define CAP_PAD 7      /* rounded arc cap (5 px) plus anti-aliasing */

static lv_obj_t *dial;
static lv_obj_t *number;
static lv_obj_t *pill;
static lv_obj_t *pill_label;
static lv_obj_t *mods[4];
static struct psptr_host host;
static char number_buf[12];

static struct {
    int32_t x;
    lv_obj_t *bar;
    lv_obj_t *dashes;
    lv_obj_t *value;
    lv_obj_t *cross;
    char buf[4];
} gauges[2] = {{.x = 20}, {.x = 252}};

static int wpm = -1;    /* latest WPM from ZMK, -1 when unknown */
static int32_t shown_x10; /* WPM currently drawn, in tenths, while gliding */
static uint8_t layer;

static const char *const mod_symbols[4] = {PSPTR_SYMBOL_CONTROL, PSPTR_SYMBOL_OPTION,
                                           PSPTR_SYMBOL_COMMAND, PSPTR_SYMBOL_SHIFT};

static float fill_of(int32_t x10) { return x10 <= 0 ? 0 : MIN((float)x10 / (WPM_MAX * 10), 1.0f); }

static void dial_draw(lv_event_t *e) {
    const struct psptr_theme *t = psptr_theme();
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t o;
    lv_obj_get_coords(dial, &o);
    int32_t cx = o.x1 + CX, cy = o.y1 + CY;
    float f = fill_of(shown_x10);
    lv_color_t accent = psptr_layer_color(layer);

    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.center.x = cx;
    arc.center.y = cy;
    arc.radius = RADIUS;
    arc.width = ARC_W;
    arc.rounded = 1;
    arc.start_angle = ARC_START;
    arc.end_angle = (ARC_START + ARC_SWEEP) % 360;
    arc.color = t->track;
    lv_draw_arc(l, &arc);

    int32_t sweep = (int32_t)lroundf(ARC_SWEEP * f);
    if (sweep > 0) {
        arc.end_angle = (ARC_START + sweep) % 360;
        arc.color = accent;
        lv_draw_arc(l, &arc);
    }

    lv_draw_line_dsc_t tick;
    lv_draw_line_dsc_init(&tick);
    for (int i = 0; i <= TICKS; i++) {
        float a = (ARC_START + (float)i * ARC_SWEEP / TICKS) * PI_F / 180.0f;
        bool major = i % 5 == 0;
        float r0 = major ? 96 : 98, r1 = 103;
        tick.p1.x = cx + (int32_t)lroundf(r0 * cosf(a));
        tick.p1.y = cy + (int32_t)lroundf(r0 * sinf(a));
        tick.p2.x = cx + (int32_t)lroundf(r1 * cosf(a));
        tick.p2.y = cy + (int32_t)lroundf(r1 * sinf(a));
        tick.width = major ? 2 : 1;
        tick.color = (f > 0 && (float)i / TICKS <= f) ? accent : t->mod_off;
        lv_draw_line(l, &tick);
    }
}

static void place_centered(lv_obj_t *label, const lv_font_t *font, const char *text, int32_t y) {
    lv_obj_set_pos(label, CX - psptr_text_width(font, text) / 2, y);
}

/* Invalidate only the slice of ring (arc, caps and ticks) between two fill fractions. */
static void invalidate_sector(float f0, float f1) {
    if (f1 < f0) {
        float tmp = f0;
        f0 = f1;
        f1 = tmp;
    }
    float a0 = ARC_START + ARC_SWEEP * f0, a1 = ARC_START + ARC_SWEEP * f1;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (float a = a0;; a += 5) {
        a = MIN(a, a1);
        float c = cosf(a * PI_F / 180.0f), s = sinf(a * PI_F / 180.0f);
        for (int r = RING_IN; r <= RING_OUT; r += RING_OUT - RING_IN) {
            x0 = MIN(x0, CX + r * c);
            x1 = MAX(x1, CX + r * c);
            y0 = MIN(y0, CY + r * s);
            y1 = MAX(y1, CY + r * s);
        }
        if (a >= a1) {
            break;
        }
    }
    lv_area_t o;
    lv_obj_get_coords(dial, &o);
    lv_area_t area = {o.x1 + (int32_t)x0 - CAP_PAD, o.y1 + (int32_t)y0 - CAP_PAD,
                      o.x1 + (int32_t)x1 + CAP_PAD, o.y1 + (int32_t)y1 + CAP_PAD};
    lv_obj_invalidate_area(dial, &area);
}

static void show_number(int32_t x10) {
    /* format into a scratch buffer: number_buf is the label's own (static) text, so comparing
     * after writing into it would always match and the label would never re-measure */
    char next[sizeof(number_buf)];
    if (wpm < 0) {
        lv_snprintf(next, sizeof(next), "-");
    } else {
        lv_snprintf(next, sizeof(next), "%d", (int)((x10 + 5) / 10));
    }
    if (strcmp(next, number_buf) != 0) {
        strcpy(number_buf, next);
        lv_label_set_text_static(number, number_buf);
        place_centered(number, &PPF_NarrowThin_64, number_buf, 72);
    }
}

static void glide_step(void *obj, int32_t x10) {
    ARG_UNUSED(obj);
    float before = fill_of(shown_x10);
    shown_x10 = x10;
    if (fill_of(x10) != before) {
        invalidate_sector(before, fill_of(x10));
    }
    show_number(x10);
}

static void glide_to(int target) {
    int32_t to = target < 0 ? 0 : target * 10;
    lv_anim_delete(dial, glide_step);
    if (to == shown_x10) {
        show_number(to);
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, dial);
    lv_anim_set_exec_cb(&a, glide_step);
    lv_anim_set_values(&a, shown_x10, to);
    lv_anim_set_duration(&a, GLIDE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void update(const struct psptr_status *s, uint32_t changed) {
    const struct psptr_theme *t = psptr_theme();
    lv_color_t accent = psptr_layer_color(s->layer);
    layer = s->layer;

    if (changed & PSPTR_CHANGED_WPM) {
        wpm = s->wpm;
        glide_to(wpm);
    }

    if (changed & PSPTR_CHANGED_LAYER) {
        static char name[24];
        const char *src = psptr_layer_name(s->layer);
        size_t i = 0;
        for (; src[i] && i < sizeof(name) - 1; i++) {
            name[i] = (src[i] >= 'a' && src[i] <= 'z') ? src[i] - 32 : src[i]; /* Gridnik has caps only */
        }
        name[i] = '\0';
        lv_label_set_text_static(pill_label, name);
        int32_t w = psptr_text_width(&FoundryGridnikMedium_20, name) + 20;
        lv_obj_set_size(pill, w, PILL_H);
        lv_obj_set_pos(pill, CX - w / 2, PILL_Y);
        lv_obj_set_style_bg_color(pill, accent, 0);
    }

    if (changed & PSPTR_CHANGED_LAYER) {
        lv_obj_invalidate(dial); /* arc and lit ticks take the layer colour */
    }

    if (changed & (PSPTR_CHANGED_LAYER | PSPTR_CHANGED_MODS)) {
        const bool held[4] = {s->ctrl, s->alt, s->gui, s->shift || s->caps_word};
        lv_label_set_text_static(mods[3], s->caps_word ? PSPTR_SYMBOL_SHIFT_FILLED : PSPTR_SYMBOL_SHIFT);
        int32_t total = 3 * MODS_GAP, w[4];
        for (int i = 0; i < 4; i++) {
            w[i] = psptr_text_width(&Symbols_Semibold_28, lv_label_get_text(mods[i]));
            total += w[i];
        }
        int32_t x = CX - total / 2;
        for (int i = 0; i < 4; i++) {
            lv_obj_set_pos(mods[i], x, MODS_Y);
            lv_obj_set_style_text_color(mods[i], held[i] ? accent : t->mod_off, 0);
            x += w[i] + MODS_GAP;
        }
    }

    if (changed & PSPTR_CHANGED_PERIPHERALS) {
        for (int i = 0; i < 2; i++) {
            bool present = i < s->peripheral_count;
            bool live = present && s->peripherals[i].connected;
            uint8_t level = s->peripherals[i].level;
            int32_t cx = gauges[i].x + 4;

            psptr_show(gauges[i].bar, live);
            psptr_show(gauges[i].value, live);
            psptr_show(gauges[i].dashes, present && !live);
            psptr_show(gauges[i].cross, present && !live);
            if (!live) {
                continue;
            }
            psptr_battery_bar_set(gauges[i].bar, level);
            lv_snprintf(gauges[i].buf, sizeof(gauges[i].buf), "%d", level);
            lv_label_set_text_static(gauges[i].value, gauges[i].buf);
            lv_obj_set_style_text_color(gauges[i].value, level < PSPTR_BATTERY_LOW ? t->low : t->text, 0);
            lv_obj_set_pos(gauges[i].value, cx - psptr_text_width(&FoundryGridnikMedium_20, gauges[i].buf) / 2, 180);
        }
    }

    if (changed & PSPTR_CHANGED_ENDPOINT) {
        psptr_host_set(&host, s->endpoint, 30, 16, PSPTR_ALIGN_LEFT);
    }
}

void psptr_view_gauge_create(lv_obj_t *page) {
    const struct psptr_theme *t = psptr_theme();
    const lv_font_t *f = &FoundryGridnikMedium_20;

    dial = psptr_obj(page, 0, 0, PSPTR_W, PSPTR_H);
    lv_obj_add_event_cb(dial, dial_draw, LV_EVENT_DRAW_MAIN, NULL);

    number = psptr_label(page, &PPF_NarrowThin_64, t->text);
    lv_obj_t *unit = psptr_label(page, f, t->dim);
    lv_label_set_text_static(unit, "WPM");
    place_centered(unit, f, "WPM", 132);

    pill = psptr_rect(page, CX, PILL_Y, 20, PILL_H, PILL_H / 2, t->text);
    pill_label = psptr_label(pill, f, t->bg);
    /* centre the cap height: the line box also reserves descender space caps never use */
    lv_obj_set_pos(pill_label, 10, psptr_cap_center_offset(f, PILL_H));

    for (int i = 0; i < 4; i++) {
        mods[i] = psptr_label(page, &Symbols_Semibold_28, t->mod_off);
        lv_label_set_text_static(mods[i], mod_symbols[i]);
    }

    for (int i = 0; i < 2; i++) {
        int32_t x = gauges[i].x, cx = x + 4;
        const char *side = i == 0 ? "L" : "R";
        lv_obj_t *lab = psptr_label(page, f, t->faint);
        lv_label_set_text_static(lab, side);
        lv_obj_set_pos(lab, cx - psptr_text_width(f, side) / 2, 38);

        gauges[i].bar = psptr_battery_bar(page, x, GAUGE_Y, 8, GAUGE_H, 4);
        gauges[i].dashes = psptr_dashes(page, x, GAUGE_Y, 8, GAUGE_H, t->red_dim);
        gauges[i].value = psptr_label(page, f, t->text);
        gauges[i].cross = psptr_label(page, &lv_font_montserrat_20, t->red);
        lv_label_set_text_static(gauges[i].cross, LV_SYMBOL_CLOSE);
        lv_obj_set_pos(gauges[i].cross, cx - psptr_text_width(&lv_font_montserrat_20, LV_SYMBOL_CLOSE) / 2, 182);
    }

    psptr_host_create(&host, page, t->faint);
    psptr_status_subscribe(update);
}
