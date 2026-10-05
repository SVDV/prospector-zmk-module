#pragma once

#include <lvgl.h>

/* Each view fills the given 280x240 page and subscribes to status updates itself. */
void psptr_view_refined_create(lv_obj_t *page);
void psptr_view_live_map_create(lv_obj_t *page);
void psptr_view_gauge_create(lv_obj_t *page);
void psptr_view_cat_create(lv_obj_t *page);

/* Layer display name (or its index when the keymap has no display-name). Not thread-safe. */
const char *psptr_layer_name(uint8_t layer_index);
