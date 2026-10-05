/*
 * View 2: live map.
 * Draws the keyboard from its zmk,physical-layout. Each key keeps a session press count shown as
 * heat (8 steps), pressed keys light up, held keys use the layer colour, and a balance bar splits the
 * session's presses between the halves. The session restarts on the first press after
 * CONFIG_PROSPECTOR_LIVE_MAP_IDLE_RESET_MIN idle minutes, and on reboot.
 */

#include <math.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>

#include <zmk/behavior.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/physical_layouts.h>

#include <fonts.h>
#include <status.h>
#include <theme.h>
#include <ui.h>
#include <views.h>

#define MAX_KEYS 64
#define MAP_X 12
#define MAP_Y 52
#define MAP_W 256
#define MAP_H 98
#define KEY_GAP 2
#define KEY_RADIUS 3
#define HEAT_STEPS 7 /* 8 levels: 0..7 */
#define LIT_MS 150      /* a tap stays lit at least this long so it's visible */
#define LIT_TICK_MS 50  /* how often to check for taps whose lit time ended */
#define BALANCE_MS 500  /* the L/R share and the total update at most twice a second */
#define HEAT_MS 2000    /* heat is a slow statistic: recolour keys every 2 s at most */
#define BAL_X 20
#define BAL_W 240
#define BAL_Y 190
#define PI_F 3.14159265f
#define IDLE_RESET_MS ((int64_t)CONFIG_PROSPECTOR_LIVE_MAP_IDLE_RESET_MIN * 60 * 1000)

/* ---------- geometry ---------- */

struct key_geo {
    float cx, cy; /* centre, relative to the map object */
    float w, h;   /* drawn size, gap already removed */
    float rad;    /* rotation, radians */
    lv_area_t bounds; /* relative to the map object, for invalidation and culling */
};

static struct key_geo geo[MAX_KEYS];
static size_t key_count;
static bool left_half[MAX_KEYS];
static uint64_t caps_word_keys;

/* ---------- session data, written by the event listener ---------- */

static struct k_spinlock lock;
static uint32_t counts[MAX_KEYS];
static uint64_t pressed;
static uint32_t press_ms[MAX_KEYS];
static int64_t last_press_ms;
static bool any_press;
static uint32_t session; /* bumped on every idle reset */
static bool started;

/* ---------- display state ---------- */

static lv_obj_t *map;
static lv_obj_t *title;
static lv_obj_t *no_layout;
static lv_obj_t *bal_left_label, *bal_right_label, *bal_left_pct, *bal_right_pct, *total;
static lv_obj_t *bal_left_bar, *bal_right_bar;
static struct psptr_host host;
static struct {
    struct psptr_battery_icon icon;
    lv_obj_t *cross;
    lv_obj_t *text;
    char buf[12];
} batt[2];
static char pct_buf[2][12];
static char total_buf[16];

static struct psptr_status status;
static uint32_t shown_counts[MAX_KEYS];
static uint8_t heat[MAX_KEYS];
static uint64_t shown_pressed;
static uint64_t lit;          /* keys shown lit because of a recent press */
static bool balance_dirty;
static bool heat_dirty;
static lv_timer_t *balance_timer;
static lv_timer_t *heat_timer;
static uint32_t shown_session;
static lv_timer_t *lit_timer;

/* ---------- geometry from the physical layout ---------- */

static void rotate(float px, float py, float ox, float oy, float rad, float *x, float *y) {
    float c = cosf(rad), s = sinf(rad);
    *x = ox + (px - ox) * c - (py - oy) * s;
    *y = oy + (px - ox) * s + (py - oy) * c;
}

static void corners(const struct zmk_key_physical_attrs *k, float out[4][2]) {
    const float xs[4] = {k->x, k->x + k->width, k->x, k->x + k->width};
    const float ys[4] = {k->y, k->y, k->y + k->height, k->y + k->height};
    float rad = 0, ox = 0, oy = 0;
#if IS_ENABLED(CONFIG_ZMK_PHYSICAL_LAYOUT_KEY_ROTATION)
    rad = k->r / 100.0f * PI_F / 180.0f;
    ox = k->rx;
    oy = k->ry;
#endif
    for (int i = 0; i < 4; i++) {
        rotate(xs[i], ys[i], ox, oy, rad, &out[i][0], &out[i][1]);
    }
}

static bool build_geometry(void) {
    struct zmk_physical_layout const *const *layouts;
    size_t n = zmk_physical_layouts_get_list(&layouts);
    int sel = zmk_physical_layouts_get_selected();
    if (n == 0 || sel < 0 || (size_t)sel >= n || layouts[sel]->keys_len == 0) {
        return false;
    }
    const struct zmk_physical_layout *layout = layouts[sel];
    key_count = MIN(layout->keys_len, MAX_KEYS);

    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (size_t i = 0; i < key_count; i++) {
        float c[4][2];
        corners(&layout->keys[i], c);
        for (int j = 0; j < 4; j++) {
            x0 = MIN(x0, c[j][0]);
            y0 = MIN(y0, c[j][1]);
            x1 = MAX(x1, c[j][0]);
            y1 = MAX(y1, c[j][1]);
        }
    }
    float u = MIN(MAP_W / (x1 - x0), MAP_H / (y1 - y0)); /* px per centi-key-unit */
    float ox = (MAP_W - (x1 - x0) * u) / 2 - x0 * u;
    float oy = (MAP_H - (y1 - y0) * u) / 2 - y0 * u;
    float mid = (x0 + x1) / 2;

    for (size_t i = 0; i < key_count; i++) {
        const struct zmk_key_physical_attrs *k = &layout->keys[i];
        float c[4][2];
        corners(k, c);
        struct key_geo *g = &geo[i];
        g->cx = ox + (c[0][0] + c[3][0]) / 2 * u;
        g->cy = oy + (c[0][1] + c[3][1]) / 2 * u;
        g->w = k->width * u - KEY_GAP;
        g->h = k->height * u - KEY_GAP;
#if IS_ENABLED(CONFIG_ZMK_PHYSICAL_LAYOUT_KEY_ROTATION)
        g->rad = k->r / 100.0f * PI_F / 180.0f;
#else
        g->rad = 0;
#endif
        float ext = (fabsf(g->w * cosf(g->rad)) + fabsf(g->h * sinf(g->rad))) / 2 + 1;
        float eyt = (fabsf(g->w * sinf(g->rad)) + fabsf(g->h * cosf(g->rad))) / 2 + 1;
        g->bounds.x1 = (int32_t)floorf(g->cx - ext);
        g->bounds.y1 = (int32_t)floorf(g->cy - eyt);
        g->bounds.x2 = (int32_t)ceilf(g->cx + ext);
        g->bounds.y2 = (int32_t)ceilf(g->cy + eyt);
        left_half[i] = (c[0][0] + c[3][0]) / 2 < mid;
    }
    return true;
}

/* Keys bound to &caps_word on the default layer light up while caps word is on. */
static void find_caps_word_keys(void) {
    zmk_keymap_layer_id_t layer = zmk_keymap_layer_default();
    for (size_t i = 0; i < key_count; i++) {
        const struct zmk_behavior_binding *b = zmk_keymap_get_layer_binding_at_idx(layer, i);
        if (b && b->behavior_dev && strcmp(b->behavior_dev, "caps_word") == 0) {
            caps_word_keys |= BIT64(i);
        }
    }
}

/* ---------- drawing ---------- */

static lv_color_t key_color(size_t i) {
    const struct psptr_theme *t = psptr_theme();
    /* thermal ramp; the cooler steps are blended toward the idle key colour so rare keys stay quiet */
    lv_color_t c = t->key;
    if (heat[i] > 0) {
        uint8_t s = heat[i] - 1; /* 0..6 */
        c = lv_color_mix(t->heat[s], t->key, (uint8_t)(110 + s * 145 / (HEAT_STEPS - 1)));
    }
    bool held = (shown_pressed & BIT64(i)) || (status.caps_word && (caps_word_keys & BIT64(i)));
    if (held) {
        c = psptr_layer_color(status.layer);
    }
    if (lit & BIT64(i)) {
        c = lv_color_mix(t->text, c, 200);
    }
    return c;
}

static bool connected(size_t i) {
    if (status.peripheral_count == 0) {
        return true;
    }
    /* one peripheral (a unibody board) covers every key; two map to the left/right halves */
    uint8_t side = (status.peripheral_count == 1 || left_half[i]) ? 0 : 1;
    return status.peripherals[side].connected;
}

static void draw_line(lv_layer_t *layer, float x1, float y1, float x2, float y2, int32_t width,
                      lv_color_t color) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1.x = (int32_t)lroundf(x1);
    d.p1.y = (int32_t)lroundf(y1);
    d.p2.x = (int32_t)lroundf(x2);
    d.p2.y = (int32_t)lroundf(y2);
    d.width = width;
    d.color = color;
    lv_draw_line(layer, &d);
}

static void draw_dot(lv_layer_t *layer, float x, float y, int32_t r, lv_color_t color) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    d.radius = LV_RADIUS_CIRCLE;
    lv_area_t a = {(int32_t)lroundf(x) - r, (int32_t)lroundf(y) - r, (int32_t)lroundf(x) + r - 1,
                   (int32_t)lroundf(y) + r - 1};
    lv_draw_rect(layer, &d, &a);
}

/* A rotated rounded rectangle as the union of two thick lines and four corner dots. */
static void draw_rotated_key(lv_layer_t *layer, float cx, float cy, const struct key_geo *g,
                             lv_color_t color, bool outline) {
    float ux = cosf(g->rad), uy = sinf(g->rad), vx = -uy, vy = ux;
    float hw = g->w / 2, hh = g->h / 2, r = KEY_RADIUS;

    if (outline) {
        float p[4][2] = {{cx - ux * hw - vx * hh, cy - uy * hw - vy * hh},
                         {cx + ux * hw - vx * hh, cy + uy * hw - vy * hh},
                         {cx + ux * hw + vx * hh, cy + uy * hw + vy * hh},
                         {cx - ux * hw + vx * hh, cy - uy * hw + vy * hh}};
        for (int i = 0; i < 4; i++) {
            draw_line(layer, p[i][0], p[i][1], p[(i + 1) % 4][0], p[(i + 1) % 4][1], 1, color);
        }
        return;
    }
    draw_line(layer, cx - ux * (hw - r), cy - uy * (hw - r), cx + ux * (hw - r), cy + uy * (hw - r),
              (int32_t)lroundf(g->h), color);
    draw_line(layer, cx - ux * hw, cy - uy * hw, cx + ux * hw, cy + uy * hw,
              (int32_t)lroundf(g->h - 2 * r), color);
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            draw_dot(layer, cx + sx * ux * (hw - r) + sy * vx * (hh - r),
                     cy + sx * uy * (hw - r) + sy * vy * (hh - r), (int32_t)r, color);
        }
    }
}

static bool overlaps(const lv_area_t *a, const lv_area_t *b) {
    return a->x1 <= b->x2 && b->x1 <= a->x2 && a->y1 <= b->y2 && b->y1 <= a->y2;
}

static void map_draw(lv_event_t *e) {
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t origin;
    lv_obj_get_coords(map, &origin);

    for (size_t i = 0; i < key_count; i++) {
        const struct key_geo *g = &geo[i];
        lv_area_t b = g->bounds;
        lv_area_move(&b, origin.x1, origin.y1);
        if (!overlaps(&b, &layer->_clip_area)) {
            continue;
        }
        bool live = connected(i);
        lv_color_t color = live ? key_color(i) : psptr_theme()->red_dim;
        float cx = origin.x1 + g->cx, cy = origin.y1 + g->cy;

        if (fabsf(g->rad) > 0.001f) {
            draw_rotated_key(layer, cx, cy, g, color, !live);
            continue;
        }
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.radius = KEY_RADIUS;
        if (live) {
            d.bg_color = color;
        } else {
            d.bg_opa = LV_OPA_TRANSP;
            d.border_color = color;
            d.border_width = 1;
        }
        lv_area_t a = {(int32_t)lroundf(cx - g->w / 2), (int32_t)lroundf(cy - g->h / 2), 0, 0};
        a.x2 = a.x1 + (int32_t)lroundf(g->w) - 1;
        a.y2 = a.y1 + (int32_t)lroundf(g->h) - 1;
        lv_draw_rect(layer, &d, &a);
    }
}

static void invalidate_key(size_t i) {
    lv_area_t a = geo[i].bounds;
    lv_area_t origin;
    lv_obj_get_coords(map, &origin);
    lv_area_move(&a, origin.x1, origin.y1);
    lv_obj_invalidate_area(map, &a);
}

static void invalidate_keys(uint64_t mask) {
    for (size_t i = 0; i < key_count && mask; i++) {
        if (mask & BIT64(i)) {
            invalidate_key(i);
            mask &= ~BIT64(i);
        }
    }
}

/* ---------- lit keys ---------- */

/* Redraw only the keys whose lit time just ended; held keys then show the layer colour. */
static void lit_tick(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    uint32_t now = k_uptime_get_32();
    uint64_t ended = 0;
    for (size_t i = 0; i < key_count; i++) {
        if ((lit & BIT64(i)) && now - press_ms[i] >= LIT_MS) {
            ended |= BIT64(i);
        }
    }
    lit &= ~ended;
    invalidate_keys(ended);
    if (!lit) {
        lv_timer_pause(lit_timer);
    }
}

/* ---------- balance and labels ---------- */

static void format_thousands(char *buf, size_t len, uint32_t v) {
    char tmp[16];
    int n = lv_snprintf(tmp, sizeof(tmp), "%u", (unsigned int)v);
    size_t o = 0;
    for (int i = 0; i < n && o + 1 < len; i++) {
        if (i > 0 && (n - i) % 3 == 0 && o + 2 < len) {
            buf[o++] = ',';
        }
        buf[o++] = tmp[i];
    }
    buf[o] = '\0';
}

static void update_balance(void) {
    const lv_font_t *f = &FoundryGridnikMedium_20;
    uint32_t left = 0, all = 0;
    for (size_t i = 0; i < key_count; i++) {
        all += shown_counts[i];
        left += left_half[i] ? shown_counts[i] : 0;
    }
    int lp = all ? (int)(((uint64_t)left * 100 + all / 2) / all) : 50;
    lp = CLAMP(lp, 0, 100);

    lv_snprintf(pct_buf[0], sizeof(pct_buf[0]), "%d%%", lp);
    lv_snprintf(pct_buf[1], sizeof(pct_buf[1]), "%d%%", 100 - lp);
    lv_label_set_text_static(bal_left_pct, pct_buf[0]);
    lv_label_set_text_static(bal_right_pct, pct_buf[1]);
    lv_obj_set_x(bal_left_pct, BAL_X + psptr_text_width(f, "L") + 7);
    lv_obj_set_x(bal_right_pct, BAL_X + BAL_W - psptr_text_width(f, "R") - 7 - psptr_text_width(f, pct_buf[1]));

    format_thousands(total_buf, sizeof(total_buf), all);
    lv_label_set_text_static(total, total_buf);
    lv_obj_set_x(total, PSPTR_W / 2 - psptr_text_width(f, total_buf) / 2);

    int32_t split = BAL_X + (BAL_W * lp + 50) / 100;
    lv_obj_set_width(bal_left_bar, LV_MAX(split - BAL_X - 1, 0));
    lv_obj_set_x(bal_right_bar, split + 1);
    lv_obj_set_width(bal_right_bar, LV_MAX(BAL_X + BAL_W - split - 1, 0));
}

/* heat = (count / max)^0.7 in 8 steps, recomputed in batches so a burst of typing repaints once.
 * Only keys whose step changed are repainted: a full-map redraw costs 40-60 ms on the device. */
static void recompute_heat(void) {
    uint32_t max = 1;
    for (size_t i = 0; i < key_count; i++) {
        max = MAX(max, shown_counts[i]);
    }
    uint64_t changed = 0;
    for (size_t i = 0; i < key_count; i++) {
        uint32_t c = shown_counts[i];
        uint8_t step = c ? (uint8_t)lroundf(powf((float)c / max, 0.7f) * HEAT_STEPS) : 0;
        if (step != heat[i]) {
            heat[i] = step;
            changed |= BIT64(i);
        }
    }
    invalidate_keys(changed & ~lit); /* lit keys pick up their new heat when they dim */
}

static void heat_tick(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    if (heat_dirty && map) {
        heat_dirty = false;
        recompute_heat();
    }
}

static void balance_tick(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    if (balance_dirty) {
        balance_dirty = false;
        update_balance();
    }
}

static void update_batteries(void) {
    const struct psptr_theme *t = psptr_theme();
    const int32_t by = 210, gap = 6;
    for (int i = 0; i < 2; i++) {
        bool present = i < status.peripheral_count;
        bool live = present && status.peripherals[i].connected;
        uint8_t level = status.peripherals[i].level;
        bool low = level < PSPTR_BATTERY_LOW;
        bool left = i == 0;

        psptr_battery_icon_set(&batt[i].icon, level, live);
        psptr_show(batt[i].cross, present && !live);
        psptr_show(batt[i].text, present);
        if (!present) {
            continue;
        }
        if (live) {
            lv_snprintf(batt[i].buf, sizeof(batt[i].buf), "%d%%", level);
        } else {
            strcpy(batt[i].buf, "NO LINK");
        }
        lv_label_set_text_static(batt[i].text, batt[i].buf);
        lv_obj_set_style_text_color(batt[i].text, !live ? t->red : low ? t->low : t->text, 0);

        int32_t icon_w = live ? PSPTR_BATTERY_ICON_W : psptr_text_width(&lv_font_montserrat_20, LV_SYMBOL_CLOSE);
        int32_t icon_x = left ? 24 : 256 - icon_w;
        int32_t text_w = psptr_text_width(&FoundryGridnikMedium_20, batt[i].buf);
        lv_obj_set_pos(batt[i].cross, icon_x, by);
        lv_obj_set_pos(batt[i].text, left ? icon_x + icon_w + gap : icon_x - gap - text_w, by - 2);
    }
}

/* ---------- key events ---------- */

static void keys_changed(struct k_work *work);
static K_WORK_DEFINE(keys_work, keys_changed);

static int on_position(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (!ev || ev->position >= MAX_KEYS) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    k_spinlock_key_t key = k_spin_lock(&lock);
    if (ev->state) {
        int64_t now = k_uptime_get();
        if (IDLE_RESET_MS > 0 && any_press && now - last_press_ms > IDLE_RESET_MS) {
            memset(counts, 0, sizeof(counts));
            session++;
        }
        counts[ev->position]++;
        pressed |= BIT64(ev->position);
        press_ms[ev->position] = (uint32_t)now;
        last_press_ms = now;
        any_press = true;
    } else {
        pressed &= ~BIT64(ev->position);
    }
    k_spin_unlock(&lock, key);

    if (started) {
        k_work_submit_to_queue(zmk_display_work_q(), &keys_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(psptr_live_map, on_position);
ZMK_SUBSCRIPTION(psptr_live_map, zmk_position_state_changed);

static void keys_changed(struct k_work *work) {
    ARG_UNUSED(work);
    uint32_t snap[MAX_KEYS];
    uint64_t now_pressed;
    uint32_t snap_session;
    k_spinlock_key_t key = k_spin_lock(&lock);
    memcpy(snap, counts, sizeof(snap));
    now_pressed = pressed;
    snap_session = session;
    k_spin_unlock(&lock, key);

    if (snap_session != shown_session) {
        /* new session: compare against zero so its first presses still count as new */
        memset(shown_counts, 0, sizeof(shown_counts));
        shown_session = snap_session;
    }

    uint64_t new_presses = 0;
    for (size_t i = 0; i < key_count; i++) {
        if (snap[i] > shown_counts[i]) {
            new_presses |= BIT64(i);
        }
    }
    /* a press or release only repaints that key; a lit key looks the same either way */
    uint64_t dirty = (now_pressed ^ shown_pressed) & ~lit;
    memcpy(shown_counts, snap, sizeof(shown_counts));
    shown_pressed = now_pressed;

    lit |= new_presses;
    invalidate_keys(dirty | new_presses);
    if (lit) {
        lv_timer_resume(lit_timer);
    }
    balance_dirty = true;
    heat_dirty = true;
}

/* ---------- status ---------- */

static void update(const struct psptr_status *s, uint32_t changed) {
    bool links_changed = false;
    for (int i = 0; i < PSPTR_MAX_PERIPHERALS; i++) {
        links_changed |= s->peripherals[i].connected != status.peripherals[i].connected;
    }
    bool caps_changed = s->caps_word != status.caps_word;
    status = *s;

    if (changed & PSPTR_CHANGED_LAYER) {
        lv_label_set_text(title, psptr_layer_name(s->layer));
        lv_obj_set_style_text_color(title, psptr_layer_color(s->layer), 0);
        invalidate_keys(shown_pressed | caps_word_keys); /* held keys use the layer colour */
    }
    if (map && (links_changed || (changed == PSPTR_CHANGED_ALL))) {
        lv_obj_invalidate(map);
    } else if (map && caps_changed) {
        invalidate_keys(caps_word_keys);
    }
    if ((changed & PSPTR_CHANGED_PERIPHERALS) || links_changed) {
        update_batteries();
    }
    if (changed & PSPTR_CHANGED_ENDPOINT) {
        psptr_host_set(&host, s->endpoint, 260, 18, PSPTR_ALIGN_RIGHT);
    }
}

void psptr_view_live_map_create(lv_obj_t *page) {
    const struct psptr_theme *t = psptr_theme();
    const lv_font_t *f = &FoundryGridnikMedium_20;

    title = psptr_label(page, &FR_Regular_30, t->text);
    lv_obj_set_pos(title, 20, 12);
    psptr_host_create(&host, page, t->dim);

    if (build_geometry()) {
        find_caps_word_keys();
        map = psptr_obj(page, MAP_X, MAP_Y, MAP_W, MAP_H);
        lv_obj_add_event_cb(map, map_draw, LV_EVENT_DRAW_MAIN, NULL);
    } else {
        no_layout = psptr_label(page, f, t->faint);
        lv_label_set_text_static(no_layout, "NO PHYSICAL LAYOUT");
        lv_obj_set_pos(no_layout, PSPTR_W / 2 - psptr_text_width(f, "NO PHYSICAL LAYOUT") / 2,
                       MAP_Y + MAP_H / 2 - lv_font_get_line_height(f) / 2);
    }

    bal_left_label = psptr_label(page, f, t->faint);
    lv_label_set_text_static(bal_left_label, "L");
    lv_obj_set_pos(bal_left_label, BAL_X, 160);
    bal_right_label = psptr_label(page, f, t->faint);
    lv_label_set_text_static(bal_right_label, "R");
    lv_obj_set_pos(bal_right_label, BAL_X + BAL_W - psptr_text_width(f, "R"), 160);
    bal_left_pct = psptr_label(page, f, t->text);
    lv_obj_set_y(bal_left_pct, 160);
    bal_right_pct = psptr_label(page, f, t->text);
    lv_obj_set_y(bal_right_pct, 160);
    total = psptr_label(page, f, t->faint);
    lv_obj_set_y(total, 160);

    bal_left_bar = psptr_rect(page, BAL_X, BAL_Y, BAL_W / 2 - 1, 6, 3, t->bal_left);
    bal_right_bar = psptr_rect(page, PSPTR_W / 2 + 1, BAL_Y, BAL_W / 2 - 1, 6, 3, t->bal_right);
    lv_obj_t *tick = psptr_rect(page, PSPTR_W / 2, BAL_Y - 4, 1, 14, 0, t->text);
    lv_obj_set_style_bg_opa(tick, 140, 0);

    for (int i = 0; i < 2; i++) {
        psptr_battery_icon_create(&batt[i].icon, page, i == 0 ? 24 : 256 - PSPTR_BATTERY_ICON_W, 213);
        batt[i].cross = psptr_label(page, &lv_font_montserrat_20, t->red);
        lv_label_set_text_static(batt[i].cross, LV_SYMBOL_CLOSE);
        batt[i].text = psptr_label(page, f, t->text);
    }

    lit_timer = lv_timer_create(lit_tick, LIT_TICK_MS, NULL);
    lv_timer_pause(lit_timer);
    balance_timer = lv_timer_create(balance_tick, BALANCE_MS, NULL);
    heat_timer = lv_timer_create(heat_tick, HEAT_MS, NULL);

    psptr_status_subscribe(update);
    started = true;
    keys_changed(NULL);
    balance_tick(NULL);
    heat_tick(NULL);
}
