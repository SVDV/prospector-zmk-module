#include <string.h>

#include <fonts.h>
#include <theme.h>
#include <ui.h>

lv_obj_t *psptr_obj(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    return obj;
}

lv_obj_t *psptr_rect(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius,
                     lv_color_t color) {
    lv_obj_t *obj = psptr_obj(parent, x, y, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    return obj;
}

void psptr_show(lv_obj_t *obj, bool visible) {
    if (visible) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *psptr_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text_static(label, "");
    return label;
}

int32_t psptr_text_width(const lv_font_t *font, const char *text) {
    return lv_text_get_width(text, strlen(text), font, 0);
}

int32_t psptr_cap_center_offset(const lv_font_t *font, int32_t box_h) {
    lv_font_glyph_dsc_t g;
    if (!lv_font_get_glyph_dsc(font, &g, 'H', 0)) {
        return (box_h - lv_font_get_line_height(font)) / 2;
    }
    int32_t cap_top = lv_font_get_line_height(font) - font->base_line - g.box_h - g.ofs_y;
    return (box_h - g.box_h) / 2 - cap_top;
}

/* ---------- dashes ---------- */

static void dashes_draw(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    bool vertical = lv_area_get_height(&a) > lv_area_get_width(&a);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_obj_get_style_bg_color(obj, 0);
    dsc.radius = 1;

    int32_t len = vertical ? lv_area_get_height(&a) : lv_area_get_width(&a);
    for (int32_t p = 0; p < len; p += 10) {
        int32_t seg = LV_MIN(6, len - p);
        lv_area_t s = a;
        if (vertical) {
            s.y1 = a.y1 + p;
            s.y2 = s.y1 + seg - 1;
        } else {
            s.x1 = a.x1 + p;
            s.x2 = s.x1 + seg - 1;
        }
        lv_draw_rect(layer, &dsc, &s);
    }
}

lv_obj_t *psptr_dashes(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t color) {
    lv_obj_t *obj = psptr_obj(parent, x, y, w, h);
    /* colour is stored as a style but the obj draws no background itself (bg_opa stays 0) */
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_add_event_cb(obj, dashes_draw, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}

/* ---------- battery bar ---------- */

lv_obj_t *psptr_battery_bar(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                            int32_t radius) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, radius, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, radius, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(bar, h > w ? LV_GRAD_DIR_VER : LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(bar, 250, 0);
    return bar;
}

void psptr_battery_bar_set(lv_obj_t *bar, uint8_t level) {
    const struct psptr_theme *t = psptr_theme();
    bool low = level < PSPTR_BATTERY_LOW;
    bool vertical = lv_obj_get_height(bar) > lv_obj_get_width(bar);
    lv_color_t from = low ? t->bar_low_from : t->bar_from;
    lv_color_t to = low ? t->bar_low_to : t->bar_to;

    lv_obj_set_style_bg_color(bar, low ? t->bar_low_track : t->track, LV_PART_MAIN);
    /* vertical bars are brightest at the top, horizontal ones at the right */
    lv_obj_set_style_bg_color(bar, vertical ? to : from, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(bar, vertical ? from : to, LV_PART_INDICATOR);
    lv_bar_set_value(bar, level, LV_ANIM_ON);
}

/* ---------- battery icon ---------- */

void psptr_battery_icon_create(struct psptr_battery_icon *icon, lv_obj_t *parent, int32_t x,
                               int32_t y) {
    icon->body = lv_bar_create(parent);
    lv_obj_remove_flag(icon->body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(icon->body, x, y);
    lv_obj_set_size(icon->body, 22, 12);
    lv_bar_set_range(icon->body, 0, 100);
    lv_obj_set_style_bg_opa(icon->body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(icon->body, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(icon->body, 3, LV_PART_MAIN);
    /* the bar insets its indicator by the padding: 1 px border + 2 px gap */
    lv_obj_set_style_pad_all(icon->body, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(icon->body, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(icon->body, 1, LV_PART_INDICATOR);

    icon->nub = psptr_rect(parent, x + 22, y + 4, 2, 4, 1, psptr_theme()->dim);
}

void psptr_battery_icon_set(struct psptr_battery_icon *icon, uint8_t level, bool visible) {
    const struct psptr_theme *t = psptr_theme();
    bool low = level < PSPTR_BATTERY_LOW;
    lv_color_t edge = low ? t->low : t->dim;

    lv_obj_set_style_border_color(icon->body, edge, LV_PART_MAIN);
    lv_obj_set_style_bg_color(icon->body, low ? t->low : t->bar_to, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(icon->nub, edge, 0);
    lv_bar_set_value(icon->body, level, LV_ANIM_OFF);

    psptr_show(icon->body, visible);
    psptr_show(icon->nub, visible);
}

/* ---------- host ---------- */

void psptr_host_create(struct psptr_host *host, lv_obj_t *parent, lv_color_t color) {
    host->glyph = psptr_label(parent, &lv_font_montserrat_20, color);
    host->profile = psptr_label(parent, &FoundryGridnikMedium_20, color);
}

void psptr_host_set(struct psptr_host *host, struct zmk_endpoint_instance endpoint, int32_t x,
                    int32_t y, enum psptr_align align) {
    bool ble = endpoint.transport == ZMK_TRANSPORT_BLE;

    lv_label_set_text_static(host->glyph, ble ? LV_SYMBOL_BLUETOOTH : LV_SYMBOL_USB);
    if (ble) {
        lv_snprintf(host->digit, sizeof(host->digit), "%d", endpoint.ble.profile_index);
        lv_label_set_text_static(host->profile, host->digit);
    }
    psptr_show(host->profile, ble);

    int32_t w1 = psptr_text_width(&lv_font_montserrat_20, lv_label_get_text(host->glyph));
    int32_t w2 = ble ? psptr_text_width(&FoundryGridnikMedium_20, host->digit) : 0;
    int32_t total = w1 + (ble ? 3 + w2 : 0);
    int32_t x0 = align == PSPTR_ALIGN_LEFT ? x : align == PSPTR_ALIGN_CENTER ? x - total / 2 : x - total;

    lv_obj_set_pos(host->glyph, x0, y);
    lv_obj_set_pos(host->profile, x0 + w1 + 3, y - 2);
}
