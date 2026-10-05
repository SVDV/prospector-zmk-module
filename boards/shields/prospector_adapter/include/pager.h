#pragma once

#include <lvgl.h>

/*
 * Full-screen pages that slide on the swipe axis chosen with CONFIG_PROSPECTOR_SWIPE_VIEWS_*.
 * Everything except psptr_pager_post_step() must run on the display thread.
 */

void psptr_pager_init(lv_obj_t *screen);

/* Create the next page container (280x240). Call between init and finish. */
lv_obj_t *psptr_pager_add_page(void);

/* Show the first page and create the page dots. */
void psptr_pager_finish(void);

/* +1 = next view, -1 = previous view (wraps around). */
void psptr_pager_step(int dir);

/* Same as psptr_pager_step(), callable from any thread (e.g. the touch input callback). */
void psptr_pager_post_step(int dir);
