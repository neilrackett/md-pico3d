/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RENDER_MATH_H
#define RENDER_MATH_H

#include <stdint.h>

/* Fixed point factor: power of 2 allows bit-shifts instead of divisions */
#define FIXED_POINT_FACTOR 1024

static inline int float_to_int(float in) {
    return (int32_t)(in * FIXED_POINT_FACTOR);
}

static inline float int_to_float(int32_t in) {
    return in * (1.0f / FIXED_POINT_FACTOR);
}

/* 4x4 floating point matrix multiply */
static inline void mat_mul(float mat1[4][4], float mat2[4][4], float out[4][4]) {
    int y, x, z;
    for (y = 0; y < 4; y++) {
        for (x = 0; x < 4; x++) {
            out[y][x] = 0.0f;
            for (z = 0; z < 4; z++) {
                out[y][x] += mat1[y][z] * mat2[z][x];
            }
        }
    }
}

/* Convert 4x4 float matrix to fixed-point integer matrix */
static inline void mat_convert_float_fixed(float mat_in[4][4], int32_t mat_out[4][4]) {
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            mat_out[i][j] = float_to_int(mat_in[i][j]);
        }
    }
}

#endif /* RENDER_MATH_H */
