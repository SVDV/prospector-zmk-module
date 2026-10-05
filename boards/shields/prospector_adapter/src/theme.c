#include <zephyr/sys/util.h>
#include <theme.h>

#define C(hex) LV_COLOR_MAKE(((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF)

#if IS_ENABLED(CONFIG_PROSPECTOR_THEME_CATPPUCCIN_MOCHA)

static const struct psptr_theme theme = {
    .bg = C(0x11111b), /* crust: the panel lifts dark shades, so base (#1e1e2e) looked too light */
    .text = C(0xcdd6f4),
    .dim = C(0x7f849c),
    .faint = C(0x585b70),
    .track = C(0x313244),
    .key = C(0x313244),
    .mod_off = C(0x45475a),
    .red = C(0xf38ba8),
    .red_dim = C(0xa65d74),
    .low = C(0xfab387),
    /* fire, from the palette: lavender, mauve, red, maroon, peach, yellow, rosewater */
    .heat = {C(0xb4befe), C(0xcba6f7), C(0xf38ba8), C(0xeba0ac), C(0xfab387), C(0xf9e2af), C(0xf5e0dc)},
    .bal_left = C(0x89b4fa),
    .bal_right = C(0xcba6f7),
    .bar_from = C(0xa6e3a1),
    .bar_to = C(0xa6e3a1),
    .bar_low_from = C(0xfab387),
    .bar_low_to = C(0xfab387),
    .bar_low_track = C(0x313244),
};

/* blue, yellow, peach, pink, green, mauve, lavender, sapphire, teal */
static const uint32_t layer_colors[] = {0x89b4fa, 0xf9e2af, 0xfab387, 0xf5c2e7, 0xa6e3a1,
                                        0xcba6f7, 0xb4befe, 0x74c7ec, 0x94e2d5};

#else

static const struct psptr_theme theme = {
    .bg = C(0x000000),
    .text = C(0xffffff),
    .dim = C(0x909090),
    .faint = C(0x5a5a5a),
    .track = C(0x202020),
    .key = C(0x1a1a1a),
    .mod_off = C(0x2e2e2e),
    .red = C(0xe63030),
    .red_dim = C(0x9e2121),
    .low = C(0xffb802),
    /* fire (inferno): purple, plum, crimson, red, orange, amber, yellow. Brighter = pressed more */
    .heat = {C(0x6a176e), C(0x932667), C(0xbc3754), C(0xdd513a), C(0xf37819), C(0xfca50a), C(0xf6d746)},
    .bal_left = C(0xf0f0f0),
    .bal_right = C(0x7a7a7a),
    .bar_from = C(0x909090),
    .bar_to = C(0xf0f0f0),
    .bar_low_from = C(0xd3900f),
    .bar_low_to = C(0xe8ac11),
    .bar_low_track = C(0x6e4e07),
};

static const uint32_t layer_colors[] = {0x8fb8ff, 0xf5e27a, 0xffb86b, 0xff8fa3, 0x7ee0b5,
                                        0xc9a0ff, 0xd0d4dc, 0x6fe3f0, 0xa8e66f};

#endif

const struct psptr_theme *psptr_theme(void) { return &theme; }

lv_color_t psptr_layer_color(uint8_t layer_index) {
    return lv_color_hex(layer_colors[layer_index % ARRAY_SIZE(layer_colors)]);
}
