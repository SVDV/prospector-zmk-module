#include <lvgl.h>
#include <zephyr/kernel.h>

#if IS_ENABLED(CONFIG_PROSPECTOR_PERF_OVERLAY)
#include <lvgl_mem.h>
#endif

#include <pager.h>
#include <status.h>
#include <theme.h>
#include <views.h>

#if IS_ENABLED(CONFIG_PROSPECTOR_PERF_OVERLAY)
/* LVGL's pool lives in Zephyr's heap, which LVGL's own memory monitor can't see. */
static void heap_tick(lv_timer_t *timer) {
    static char buf[48];
    struct sys_memory_stats st;
    lvgl_heap_stats(&st);
    lv_snprintf(buf, sizeof(buf), "heap %u.%uk  peak %u.%uk / %uk",
                (unsigned)(st.allocated_bytes / 1024), (unsigned)(st.allocated_bytes % 1024 * 10 / 1024),
                (unsigned)(st.max_allocated_bytes / 1024),
                (unsigned)(st.max_allocated_bytes % 1024 * 10 / 1024), (unsigned)(CONFIG_LV_Z_MEM_POOL_SIZE / 1024));
    lv_label_set_text_static(lv_timer_get_user_data(timer), buf);
}
#endif

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
    lv_obj_t *heap = lv_label_create(sys);
    lv_obj_set_style_text_color(heap, lv_color_white(), 0);
    lv_obj_set_style_bg_color(heap, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(heap, LV_OPA_50, 0);
    lv_obj_align(heap, LV_ALIGN_TOP_MID, 0, 48);
    lv_timer_create(heap_tick, 500, heap);
#endif
    return screen;
}
