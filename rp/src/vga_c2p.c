/**
 * vga_c2p.c — Chunky-to-planar conversion with 4x4 Bayer dithering
 *
 * Converts a 160x100 RGB4444 chunky framebuffer to a 320x200 Atari ST
 * 4-bitplane planar framebuffer with pixel doubling (each source pixel
 * becomes a 2x2 block in the output).
 *
 * The planar layout follows Atari ST low-res:
 *   320 pixels wide = 20 × 16-pixel "word groups"
 *   Each word group = 4 planes × 2 bytes = 8 bytes = one uint64_t
 *
 * pixel_masks_flat[palette_index << 4 | pixel_x_within_group] gives the
 * 64-bit mask to OR into the word group for one pixel.
 */

#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "engine/render_globals.h"
#include "vga/vga.h"

/* Precomputed C2P bit masks in SCRATCH_X bank for maximum performance.
 * pixel_masks_flat[palette_index<<4 | pixel_x_in_group] gives the 64-bit
 * pattern to OR into a 16-pixel word group (4 planes × 2 bytes = 8 bytes). */
uint64_t pixel_masks_flat[256]
    __attribute__((aligned(8), section(".scratch_x.pixel_masks")));

void __not_in_flash_func(init_pixel_masks)(void) {
    for (int index = 0; index < 16; index++) {
        for (int x = 0; x < 16; x++) {
            uint64_t mask = 0;
            for (int plane = 0; plane < 4; plane++) {
                if (index & (1 << plane)) {
                    mask |= (uint64_t)1 << (plane * 16 + (15 - x));
                }
            }
            pixel_masks_flat[(index << 4) | x] = mask;
        }
    }
}

/* 4x4 Bayer ordered dither matrix (values 0..15, scaled from 4-bit channel) */
static const int8_t bayer4x4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 },
};

/* Active palette LUT: maps packed RGB444 key (g<<8 | b<<4 | r, 0..4095)
 * to 4-bit palette index. Points into flash — swapped when phase changes. */
static const uint8_t *active_lut = NULL;

/* Active 16-color ST palette: 16 uint16_t words in ST format.
 * Written to shared memory each frame for the ST to load into hardware. */
static const uint16_t *active_palette = NULL;

void c2p_set_lut(const uint8_t *lut, const uint16_t *palette) {
    active_lut     = lut;
    active_palette = palette;
}

const uint16_t *c2p_get_palette(void) {
    return active_palette;
}

/**
 * c2p_convert_and_double - Convert chunky RGB4444 160x100 to ST planar 320x200
 *
 * @src  : RGB4444 chunky buffer, SCREEN_WIDTH * SCREEN_HEIGHT uint16_t pixels
 * @dst  : ST planar framebuffer (hidden), must be 320*200/2 bytes = 32000 bytes
 *
 * The output framebuffer is cleared before conversion, then each source pixel
 * is dithered, looked up via active_lut, and written as a 2x2 pixel block.
 *
 * Pixels within each 16-pixel word group are written using pixel_masks_flat
 * (same mechanism as the sprites demo's tile renderer).
 */
void __not_in_flash_func(c2p_convert_and_double)(
        const uint16_t *src, unsigned int *dst) {

    /* Clear destination framebuffer */
    memset(dst, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT / 2);  /* 4bpp = 1/2 byte per pixel */

    if (active_lut == NULL) return;

    /* Bytes per planar row at 320px, 4bpp:
     * 320px / 8 bits_per_byte * 4 planes = 160 bytes
     * but as uint64_t groups: 320/16 = 20 groups per row, each 8 bytes */
    const int GROUPS_PER_ROW = DISPLAY_WIDTH / 16;  /* 20 */
    const int BYTES_PER_ROW  = GROUPS_PER_ROW * 8;  /* 160 */

    for (int sy = 0; sy < SCREEN_HEIGHT; sy++) {
        /* Two destination rows for each source row */
        uint8_t *dst_row0 = (uint8_t *)dst + (sy * 2    ) * BYTES_PER_ROW;
        uint8_t *dst_row1 = (uint8_t *)dst + (sy * 2 + 1) * BYTES_PER_ROW;

        /* Dither row index */
        int by = sy & 3;

        for (int sx = 0; sx < SCREEN_WIDTH; sx++) {
            uint16_t rgb = src[sy * SCREEN_WIDTH + sx];

            /* Extract RGB4444 channels (Pico3D format: GBAR = g<<12|b<<8|r) */
            int r = (int)( rgb        & 0x000F);
            int b = (int)((rgb >> 8 ) & 0x000F);
            int g = (int)((rgb >> 12) & 0x000F);

            /* Apply Bayer dither: adds ordered offset to reduce quantisation */
            int bx = sx & 3;
            int dither = bayer4x4[by][bx];

            /* Dither with clamping (channels are 0..15, dither 0..15) */
            r = r + ((dither - 8) >> 2); if (r < 0) r = 0; if (r > 15) r = 15;
            g = g + ((dither - 8) >> 2); if (g < 0) g = 0; if (g > 15) g = 15;
            b = b + ((dither - 8) >> 2); if (b < 0) b = 0; if (b > 15) b = 15;

            /* Pack channels to a dense 12-bit LUT key (g<<8 | b<<4 | r). */
            uint16_t key = ((uint16_t)g << 8) | ((uint16_t)b << 4) | (uint16_t)r;
            uint8_t idx  = active_lut[key];

            /* Destination x = sx*2, pixel position within 16-pixel group */
            int dx0 = sx * 2;
            int dx1 = dx0 + 1;

            unsigned group0 = (unsigned)dx0 >> 4;
            unsigned group1 = (unsigned)dx1 >> 4;
            unsigned pos0   = (unsigned)dx0 & 0xF;
            unsigned pos1   = (unsigned)dx1 & 0xF;

            uint64_t mask0 = pixel_masks_flat[(idx << 4) | pos0];
            uint64_t mask1 = pixel_masks_flat[(idx << 4) | pos1];

            /* Write to both destination rows (vertical doubling) */
            uint64_t *blk0_r0 = (uint64_t *)(dst_row0 + group0 * 8);
            uint64_t *blk1_r0 = (uint64_t *)(dst_row0 + group1 * 8);
            uint64_t *blk0_r1 = (uint64_t *)(dst_row1 + group0 * 8);
            uint64_t *blk1_r1 = (uint64_t *)(dst_row1 + group1 * 8);

            *blk0_r0 |= mask0;
            *blk1_r0 |= mask1;
            *blk0_r1 |= mask0;
            *blk1_r1 |= mask1;
        }
    }
}
