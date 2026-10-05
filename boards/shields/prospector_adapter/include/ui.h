#pragma once

#include <lvgl.h>
#include <zmk/endpoints_types.h>

#include <status.h>

/* Screen size after the 270° rotation. All layouts are designed in these pixels. */
#define PSPTR_W 280
#define PSPTR_H 240

/* A bare object: no styles, no scrolling, transparent. (No LVGL theme is active.) */
lv_obj_t *psptr_obj(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);

void psptr_show(lv_obj_t *obj, bool visible);

/* Filled rounded rectangle. */
lv_obj_t *psptr_rect(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius,
                     lv_color_t color);

/* Label whose position is its line box top-left, like LVGL positions labels. */
lv_obj_t *psptr_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color);

int32_t psptr_text_width(const lv_font_t *font, const char *text);

/* Y offset that centres a font's cap height (not its line box) inside a box of height box_h. */
int32_t psptr_cap_center_offset(const lv_font_t *font, int32_t box_h);

/* 6 px on / 4 px off dashes along the longer side, used for a disconnected battery. */
lv_obj_t *psptr_dashes(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t color);

/* Battery fill bar (lv_bar) with the theme's track and gradient. Vertical when h > w. */
lv_obj_t *psptr_battery_bar(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                            int32_t radius);
void psptr_battery_bar_set(lv_obj_t *bar, uint8_t level);

/* Battery icon: 22x12 body, 1 px border, even 2 px gap around the fill, 2x4 terminal. */
struct psptr_battery_icon {
    lv_obj_t *body;
    lv_obj_t *nub;
};
#define PSPTR_BATTERY_ICON_W 24
void psptr_battery_icon_create(struct psptr_battery_icon *icon, lv_obj_t *parent, int32_t x,
                               int32_t y);
void psptr_battery_icon_set(struct psptr_battery_icon *icon, uint8_t level, bool visible);

/* Bottom battery row: icon + percentage for each half, left and right, with ✕ NO LINK when a
 * half is disconnected. Used by the live map and the cat. */
struct psptr_battery_pair {
    struct {
        struct psptr_battery_icon icon;
        lv_obj_t *cross;
        lv_obj_t *text;
        char buf[12];
    } side[2];
};
void psptr_battery_pair_create(struct psptr_battery_pair *pair, lv_obj_t *parent);
void psptr_battery_pair_set(struct psptr_battery_pair *pair, const struct psptr_status *s);

/* Host indicator: USB glyph, or Bluetooth glyph plus profile number. */
enum psptr_align { PSPTR_ALIGN_LEFT, PSPTR_ALIGN_CENTER, PSPTR_ALIGN_RIGHT };
struct psptr_host {
    lv_obj_t *glyph;
    lv_obj_t *profile;
    char digit[4];
};
void psptr_host_create(struct psptr_host *host, lv_obj_t *parent, lv_color_t color);
/* x is the left edge, centre or right edge depending on align; y is the glyph's line top. */
void psptr_host_set(struct psptr_host *host, struct zmk_endpoint_instance endpoint, int32_t x,
                    int32_t y, enum psptr_align align);

/* Modifier glyphs from the SF Symbols fonts. */
#define PSPTR_SYMBOL_COMMAND "\xF4\x80\x86\x94"
#define PSPTR_SYMBOL_OPTION "\xF4\x80\x86\x95"
#define PSPTR_SYMBOL_CONTROL "\xF4\x80\x86\x8D"
#define PSPTR_SYMBOL_SHIFT "\xF4\x80\x86\x9D"
#define PSPTR_SYMBOL_SHIFT_FILLED "\xF4\x80\x86\x9E"
