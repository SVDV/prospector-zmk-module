/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#define DISP_BRI_TOG_CMD 0
#define DISP_BRI_INC_CMD 1
#define DISP_BRI_DEC_CMD 2
#define DISP_BRI_SET_CMD 3

#define DISP_BRI_TOG DISP_BRI_TOG_CMD 0
#define DISP_BRI_INC DISP_BRI_INC_CMD 0
#define DISP_BRI_DEC DISP_BRI_DEC_CMD 0
#define DISP_BRI_SET(val) DISP_BRI_SET_CMD val
