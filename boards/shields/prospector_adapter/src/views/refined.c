/*
 * View 1: the original layer roller, refined.
 * Full-width roller (FRAC is 32 px per letter, so 8-letter names need 256 px), mods in a row in
 * home-row order, labelled L/R battery cells and the host glyph between them.
 */

#include <ctype.h>
#include <string.h>

#include <zmk/keymap.h>

#include <fonts.h>
#include <status.h>
#include <theme.h>
#include <ui.h>
#include <views.h>

#define ROLLER_Y 8
#define ROLLER_H 140
#define ROLLER_PAD_LEFT 14
#define FADE_H 40
#define MODS_Y 150
#define MODS_X 14
#define MODS_PITCH 55
#define CELL_Y 192
#define CELL_H 36
#define CELL_W 94

static char options[512];
static lv_obj_t *roller;
static lv_obj_t *mods[4];
static struct psptr_host host;

static struct {
    int32_t x;
    lv_obj_t *side;
    lv_obj_t *value;
    lv_obj_t *cross;
    lv_obj_t *bar;
    lv_obj_t *dashes;
    char text[4];
} cells[2] = {{.x = 16}, {.x = 170}};

/* ctrl, alt, gui, shift: the order of the home-row mods */
static const char *const mod_symbols[4] = {PSPTR_SYMBOL_CONTROL, PSPTR_SYMBOL_OPTION,
                                           PSPTR_SYMBOL_COMMAND, PSPTR_SYMBOL_SHIFT};

const char *psptr_layer_name(uint8_t layer_index) {
    static char buf[16];
    const char *name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(layer_index));
    if (name && *name) {
        return name;
    }
    lv_snprintf(buf, sizeof(buf), "%d", layer_index);
    return buf;
}

static void build_options(void) {
    char *p = options, *end = options + sizeof(options) - 1;
    for (int i = 0; i < ZMK_KEYMAP_LAYERS_LEN && p < end; i++) {
        if (i > 0) {
            *p++ = '\n';
        }
        const char *name = psptr_layer_name(i);
        while (*name && p < end) {
            *p++ = IS_ENABLED(CONFIG_PROSPECTOR_LAYER_ROLLER_ALL_CAPS) ? toupper((unsigned char)*name)
                                                                      : *name;
            name++;
        }
    }
    *p = '\0';
}

static lv_obj_t *fade(lv_obj_t *parent, lv_align_t align, bool to_bottom) {
    lv_obj_t *obj = psptr_obj(parent, 0, 0, LV_PCT(100), FADE_H);
    lv_obj_align(obj, align, 0, 0);
    lv_obj_set_style_bg_color(obj, psptr_theme()->bg, 0);
    lv_obj_set_style_bg_grad_color(obj, psptr_theme()->bg, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_main_opa(obj, to_bottom ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_opa(obj, to_bottom ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    return obj;
}

static void update(const struct psptr_status *s, uint32_t changed) {
    const struct psptr_theme *t = psptr_theme();
    lv_color_t accent = psptr_layer_color(s->layer);

    if (changed & PSPTR_CHANGED_LAYER) {
        lv_roller_set_selected(roller, s->layer, LV_ANIM_ON);
        lv_obj_set_style_text_color(roller, accent, LV_PART_SELECTED);
    }

    if (changed & (PSPTR_CHANGED_LAYER | PSPTR_CHANGED_MODS)) {
        const bool held[4] = {s->ctrl, s->alt, s->gui, s->shift || s->caps_word};
        lv_label_set_text_static(mods[3], s->caps_word ? PSPTR_SYMBOL_SHIFT_FILLED : PSPTR_SYMBOL_SHIFT);
        for (int i = 0; i < 4; i++) {
            lv_obj_set_style_text_color(mods[i], held[i] ? accent : t->mod_off, 0);
        }
    }

    if (changed & PSPTR_CHANGED_PERIPHERALS) {
        for (int i = 0; i < 2; i++) {
            bool present = i < s->peripheral_count;
            bool connected = present && s->peripherals[i].connected;
            uint8_t level = s->peripherals[i].level;

            psptr_show(cells[i].side, present);
            psptr_show(cells[i].value, connected);
            psptr_show(cells[i].bar, connected);
            psptr_show(cells[i].cross, present && !connected);
            psptr_show(cells[i].dashes, present && !connected);
            if (!connected) {
                continue;
            }
            lv_snprintf(cells[i].text, sizeof(cells[i].text), "%d", level);
            lv_label_set_text_static(cells[i].value, cells[i].text);
            lv_obj_set_style_text_color(cells[i].value, level < PSPTR_BATTERY_LOW ? t->low : t->text, 0);
            lv_obj_set_x(cells[i].value,
                         cells[i].x + CELL_W - psptr_text_width(&FoundryGridnikMedium_20, cells[i].text));
            psptr_battery_bar_set(cells[i].bar, level);
        }
    }

    if (changed & PSPTR_CHANGED_ENDPOINT) {
        psptr_host_set(&host, s->endpoint, PSPTR_W / 2,
                       CELL_Y + (CELL_H - lv_font_get_line_height(&lv_font_montserrat_20)) / 2,
                       PSPTR_ALIGN_CENTER);
    }
}

void psptr_view_refined_create(lv_obj_t *page) {
    const struct psptr_theme *t = psptr_theme();

    build_options();
    roller = lv_roller_create(page);
    lv_obj_remove_flag(roller, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(roller, 0, ROLLER_Y);
    lv_obj_set_size(roller, PSPTR_W, ROLLER_H);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_INFINITE);
    lv_obj_set_style_bg_color(roller, t->bg, 0);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(roller, 0, 0);
    lv_obj_set_style_pad_all(roller, 0, 0);
    lv_obj_set_style_pad_left(roller, ROLLER_PAD_LEFT, 0);
    lv_obj_set_style_text_font(roller, &FRAC_Thin_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, t->dim, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, &FRAC_Regular_48, LV_PART_SELECTED);
    lv_obj_set_style_anim_duration(roller, 50, 0);
    fade(roller, LV_ALIGN_TOP_MID, false);
    fade(roller, LV_ALIGN_BOTTOM_MID, true);

    for (int i = 0; i < 4; i++) {
        mods[i] = psptr_label(page, &Symbols_Semibold_32, t->mod_off);
        lv_label_set_text_static(mods[i], mod_symbols[i]);
        lv_obj_set_pos(mods[i], MODS_X + i * MODS_PITCH, MODS_Y);
    }

    int32_t text_y = CELL_Y + (CELL_H - lv_font_get_line_height(&FoundryGridnikMedium_20)) / 2;
    int32_t cross_y = CELL_Y + (CELL_H - lv_font_get_line_height(&lv_font_montserrat_20)) / 2;
    for (int i = 0; i < 2; i++) {
        int32_t x = cells[i].x;
        cells[i].side = psptr_label(page, &FoundryGridnikMedium_20, t->faint);
        lv_label_set_text_static(cells[i].side, i == 0 ? "L" : "R");
        lv_obj_set_pos(cells[i].side, x, text_y);

        cells[i].value = psptr_label(page, &FoundryGridnikMedium_20, t->text);
        lv_obj_set_pos(cells[i].value, x + CELL_W, text_y);

        cells[i].cross = psptr_label(page, &lv_font_montserrat_20, t->red);
        lv_label_set_text_static(cells[i].cross, LV_SYMBOL_CLOSE);
        lv_obj_set_pos(cells[i].cross,
                       x + CELL_W - psptr_text_width(&lv_font_montserrat_20, LV_SYMBOL_CLOSE), cross_y);

        cells[i].bar = psptr_battery_bar(page, x, CELL_Y + CELL_H - 4, CELL_W, 4, 2);
        cells[i].dashes = psptr_dashes(page, x, CELL_Y + CELL_H - 4, CELL_W, 4, t->red_dim);
    }

    psptr_host_create(&host, page, t->faint);
    psptr_status_subscribe(update);
}
