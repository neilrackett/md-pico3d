/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Day/night cycle: updates sky colour and lighting falloff based on time */
#include "logic_globals.h"
#include "../engine/render_globals.h"

uint32_t global_time;
int8_t daylight = 0;

void logic_day_night_cycle(void) {

    uint8_t sky_r, sky_g, sky_b;

    daylight = (int8_t)((global_time % (QUARTER_DAY * 4)) / QUARTER_DAY);
    int32_t progress = (int32_t)(global_time % QUARTER_DAY);

    if (daylight == 0) {
        sky_r = DAY_R;
        sky_g = DAY_G;
        sky_b = DAY_B;
        light_falloff = 0;

    } else if (daylight == 1) {
        int32_t r = ((NIGHT_R - DAY_R) * progress) / QUARTER_DAY;
        int32_t g = ((NIGHT_G - DAY_G) * progress) / QUARTER_DAY;
        int32_t b = ((NIGHT_B - DAY_B) * progress) / QUARTER_DAY;
        sky_r = (uint8_t)(DAY_R + r);
        sky_g = (uint8_t)(DAY_G + g);
        sky_b = (uint8_t)(DAY_B + b);
        light_falloff = (int8_t)((MAX_FALLOFF * progress) / QUARTER_DAY);

    } else if (daylight == 2) {
        sky_r = NIGHT_R;
        sky_g = NIGHT_G;
        sky_b = NIGHT_B;
        light_falloff = MAX_FALLOFF;

    } else {
        int32_t r = ((DAY_R - NIGHT_R) * progress) / QUARTER_DAY;
        int32_t g = ((DAY_G - NIGHT_G) * progress) / QUARTER_DAY;
        int32_t b = ((DAY_B - NIGHT_B) * progress) / QUARTER_DAY;
        sky_r = (uint8_t)(NIGHT_R + r);
        sky_g = (uint8_t)(NIGHT_G + g);
        sky_b = (uint8_t)(NIGHT_B + b);
        light_falloff = (int8_t)((MAX_FALLOFF * (QUARTER_DAY - progress)) / QUARTER_DAY);
    }

    sky = sky_g;
    sky <<= 4;
    sky |= sky_b;
    sky <<= 8;
    sky |= sky_r;
}
