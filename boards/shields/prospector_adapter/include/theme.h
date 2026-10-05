#pragma once

#include <lvgl.h>

/* Colours shared by all views. Pick the palette with CONFIG_PROSPECTOR_THEME_*. */
struct psptr_theme {
    lv_color_t bg;
    lv_color_t text;
    lv_color_t dim;     /* secondary text, unselected roller rows */
    lv_color_t faint;   /* labels like L / R, host glyph */
    lv_color_t track;   /* empty part of bars */
    lv_color_t key;     /* idle key on the live map */
    lv_color_t mod_off; /* modifier symbol when not held */
    lv_color_t red;
    lv_color_t red_dim;
    lv_color_t low;     /* battery under 20% */
    lv_color_t heat;    /* most-pressed key on the live map */
    lv_color_t bal_left;
    lv_color_t bal_right;
    lv_color_t bar_from, bar_to;         /* battery fill gradient */
    lv_color_t bar_low_from, bar_low_to; /* battery fill gradient under 20% */
    lv_color_t bar_low_track;
};

const struct psptr_theme *psptr_theme(void);
lv_color_t psptr_layer_color(uint8_t layer_index);

#define PSPTR_BATTERY_LOW 20
