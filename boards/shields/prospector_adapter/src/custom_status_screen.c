#include <lvgl.h>
#include <zephyr/kernel.h>

#include <pager.h>
#include <status.h>
#include <theme.h>
#include <views.h>

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, psptr_theme()->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    psptr_pager_init(screen);
    psptr_view_refined_create(psptr_pager_add_page());
#if IS_ENABLED(CONFIG_PROSPECTOR_VIEW_LIVE_MAP)
    psptr_view_live_map_create(psptr_pager_add_page());
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_VIEW_GAUGE)
    psptr_view_gauge_create(psptr_pager_add_page());
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_VIEW_CAT)
    psptr_view_cat_create(psptr_pager_add_page());
#endif
    psptr_pager_finish();

    psptr_status_start();

#if IS_ENABLED(CONFIG_PROSPECTOR_PERF_OVERLAY)
    /* LVGL puts the monitor bottom-right, where the panel's rounded corner hides it */
    lv_obj_t *sys = lv_layer_sys();
    for (uint32_t i = 0; i < lv_obj_get_child_count(sys); i++) {
        lv_obj_align(lv_obj_get_child(sys, i), LV_ALIGN_TOP_MID, 0, 0);
    }
#endif
    return screen;
}
