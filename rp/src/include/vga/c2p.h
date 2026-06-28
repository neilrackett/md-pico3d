/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef VGA_C2P_H
#define VGA_C2P_H

#include <stdint.h>

/* Precomputed C2P pixel masks (in SCRATCH_X) */
extern uint64_t pixel_masks_flat[256];

/** Must be called once before c2p_convert_and_double() */
void init_pixel_masks(void);

/**
 * Set the active palette LUT and 16-color ST palette.
 * @lut     : 4096-entry table mapping RGB4444 index to 4-bit palette index
 * @palette : 16 uint16_t words in Atari ST hardware format
 */
void c2p_set_lut(const uint8_t *lut, const uint16_t *palette);

/** Returns pointer to the current 16-color palette (for shared memory write) */
const uint16_t *c2p_get_palette(void);

/**
 * Convert and pixel-double a 160x100 RGB4444 chunky buffer to a 320x200
 * Atari ST 4-bitplane planar framebuffer with 4x4 Bayer dithering.
 *
 * @src : SCREEN_WIDTH*SCREEN_HEIGHT uint16_t RGB4444 pixels
 * @dst : hidden planar framebuffer (32000 bytes, word-aligned)
 */
void c2p_convert_and_double(const uint16_t *src, unsigned int *dst);

#endif /* VGA_C2P_H */
