#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Which half of the keyboard a key position is on, from the selected zmk,physical-layout:
 * a key is on the left when its (rotated) centre is left of the layout's horizontal midpoint.
 * Without a physical layout every key counts as left.
 */
bool psptr_layout_is_left(uint32_t position);
