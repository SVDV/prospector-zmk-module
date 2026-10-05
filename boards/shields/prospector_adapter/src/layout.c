#include <math.h>

#include <zephyr/sys/util.h>
#include <zmk/physical_layouts.h>

#include <layout.h>

#define MAX_KEYS 64
#define PI_F 3.14159265f

static uint64_t left_keys;
static bool built;

static void key_corners(const struct zmk_key_physical_attrs *k, float out[4][2]) {
    const float xs[4] = {k->x, k->x + k->width, k->x, k->x + k->width};
    const float ys[4] = {k->y, k->y, k->y + k->height, k->y + k->height};
    float rad = 0, ox = 0, oy = 0;
#if IS_ENABLED(CONFIG_ZMK_PHYSICAL_LAYOUT_KEY_ROTATION)
    rad = k->r / 100.0f * PI_F / 180.0f;
    ox = k->rx;
    oy = k->ry;
#endif
    float c = cosf(rad), s = sinf(rad);
    for (int i = 0; i < 4; i++) {
        out[i][0] = ox + (xs[i] - ox) * c - (ys[i] - oy) * s;
        out[i][1] = oy + (xs[i] - ox) * s + (ys[i] - oy) * c;
    }
}

static void build(void) {
    built = true;
    struct zmk_physical_layout const *const *layouts;
    size_t n = zmk_physical_layouts_get_list(&layouts);
    int sel = zmk_physical_layouts_get_selected();
    if (n == 0 || sel < 0 || (size_t)sel >= n || layouts[sel]->keys_len == 0) {
        left_keys = UINT64_MAX;
        return;
    }
    const struct zmk_physical_layout *layout = layouts[sel];
    size_t count = MIN(layout->keys_len, MAX_KEYS);

    float x0 = 1e9f, x1 = -1e9f, cx[MAX_KEYS];
    for (size_t i = 0; i < count; i++) {
        float c[4][2];
        key_corners(&layout->keys[i], c);
        for (int j = 0; j < 4; j++) {
            x0 = MIN(x0, c[j][0]);
            x1 = MAX(x1, c[j][0]);
        }
        cx[i] = (c[0][0] + c[3][0]) / 2;
    }
    float mid = (x0 + x1) / 2;
    for (size_t i = 0; i < count; i++) {
        if (cx[i] < mid) {
            left_keys |= BIT64(i);
        }
    }
}

bool psptr_layout_is_left(uint32_t position) {
    if (!built) {
        build();
    }
    return position < MAX_KEYS && (left_keys & BIT64(position));
}
