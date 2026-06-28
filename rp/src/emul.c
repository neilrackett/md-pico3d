/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * emul.c — Pico3D main loop integrated with SidecarTridge ROM emulation
 *
 * Architecture:
 *   Core 0: VBLANK-driven game loop (50 Hz from ST)
 *             → logic update, triangle list build, C2P, palette + text overlay
 *   Core 1: Rasterizer (render_rasterize → RGB4444 chunky buffer)
 *
 * Communication:
 *   ST → RP2040: ROM3 access to 0xDCBA triggers sem_release(&draw_sem)
 *   RP2040 → ST: planar framebuffer written into ROM_IN_RAM; framebuffer ID
 *                and 16-color palette written to shared offsets
 */

#include "emul.h"

#include <string.h>

#include "data/font6x8.h"
#include "memfunc.h"
#include "target_firmware.h"
#include "vga/c2p.h"

#include "engine/render_globals.h"
#include "engine/chunk_globals.h"
#include "game/logic_globals.h"
#include "palette_data.h"

#if defined(TEST_IMAGE_MODE) && (TEST_IMAGE_MODE != 0)
#define TEST_IMAGE_MODE_ACTIVE 1
#include "test_mode_image.h"
#else
#define TEST_IMAGE_MODE_ACTIVE 0
#endif

/* ──────────────────────────────────────────────────────────────────────────
 * Shared-memory layout in ROM_IN_RAM (relative to __rom_in_ram_start__)
 * 0x000–0x4FF : MOVEM.L copy-code block
 * 0x500–0x50F : 16-byte IKBD key state bitmap (written by ST assembly)
 * 0x510       : ESC exit enable flag (RP2040->ST, non-zero enables ESC->Booster)
 * 0x520–0x53F : 16 × uint16_t ST palette words (written by Core 0 each frame)
 * 0x5F8       : display command (RP2040 → ST)
 * 0x5FC       : framebuffer index (0 or 1, RP2040 → ST)
 * 0xF000–…   : terminal protocol shared variables
 * ────────────────────────────────────────────────────────────────────────── */
#define CUSTOM_FRAMEBUFFER_INDEX 0x5FC
#define CUSTOM_DISPLAY_COMMAND   0x5F8
#define ST_CMD_KEY_EVENT_BASE    0x9000u
#define ST_CMD_KEY_EVENT_MASK    0xFF80u
#define ST_CMD_KEY_EVENT_BITS    0x007Fu
#define KEY_EVENT_HOLD_FRAMES    20u

static semaphore_t draw_sem;
static semaphore_t start_demo_sem;
static volatile bool startBooster = false;
static volatile uint8_t key_event_ttl[128];

static uint32_t memorySharedAddress     = 0;
static uint32_t displayCommandAddress   = 0;

#if !TEST_IMAGE_MODE_ACTIVE
/* RGB4444 chunky framebuffer (160×100) — filled by Core 1 rasterizer */
static uint16_t chunky_fb[SCREEN_WIDTH * SCREEN_HEIGHT];
#endif

/* Defined here; render_globals.h declares it extern */
uint8_t skip_frame = 0;

/* Font RAM copy (font6x8 data must be in RAM for Core 1 access) */
static uint8_t  font6x8_data_ram[sizeof(font6x8_data_flash)];
static struct VGA_FONT font6x8_ram;

/* Track last daylight phase to detect transitions */
static int8_t last_daylight = -1;

#if TEST_IMAGE_MODE_ACTIVE
/* Fixed EGA 16-color palette in Pico3D RGB4444 GBAR layout (g<<12|b<<8|r). */
static const uint16_t test_mode_ega_gbar[16] = {
    0x0000, 0x0A00, 0xA000, 0xAA00,
    0x000A, 0x0A0A, 0x500A, 0xAA0A,
    0x5505, 0x5F05, 0xF505, 0xFF05,
    0x550F, 0x5F0F, 0xF50F, 0xFF0F
};

/* LUT key is packed RGB444: (g<<8 | b<<4 | r), range 0..4095. */
static uint8_t test_mode_lut[4096];
static uint16_t test_mode_palette_st[16];

/* Atari ST IKBD scancode for ESC; bitmap bit = scancode&7 in byte scancode>>3. */
#define TEST_MODE_ESC_SCANCODE 0x01

_Static_assert(TEST_MODE_IMAGE_WIDTH == SCREEN_WIDTH,
               "TEST_IMAGE_MODE image width must match SCREEN_WIDTH");
_Static_assert(TEST_MODE_IMAGE_HEIGHT == SCREEN_HEIGHT,
               "TEST_IMAGE_MODE image height must match SCREEN_HEIGHT");

static uint16_t rgb4444_gbar_to_st_word(uint16_t gbar) {
    uint16_t r =  gbar        & 0x0F;
    uint16_t b = (gbar >> 8)  & 0x0F;
    uint16_t g = (gbar >> 12) & 0x0F;
    return (uint16_t)(((r >> 1) << 8) | ((g >> 1) << 4) | (b >> 1));
}

/* Build fixed ST palette words and RGB4444->index LUT for test mode. */
static void build_test_mode_palette_and_lut(void) {
    uint8_t pal_r[16];
    uint8_t pal_g[16];
    uint8_t pal_b[16];

    for (int i = 0; i < 16; i++) {
        uint16_t gbar = test_mode_ega_gbar[i];
        pal_r[i] = (uint8_t)( gbar        & 0x0F);
        pal_b[i] = (uint8_t)((gbar >> 8)  & 0x0F);
        pal_g[i] = (uint8_t)((gbar >> 12) & 0x0F);
        test_mode_palette_st[i] = rgb4444_gbar_to_st_word(gbar);
    }

    for (int key = 0; key < 4096; key++) {
        int r =  key        & 0x0F;
        int b = (key >> 4)  & 0x0F;
        int g = (key >> 8)  & 0x0F;

        int best_index = 0;
        int best_dist = 1 << 30;

        for (int i = 0; i < 16; i++) {
            int dr = r - pal_r[i];
            int dg = g - pal_g[i];
            int db = b - pal_b[i];
            int dist = (dr * dr) + (dg * dg) + (db * db);

            if (dist < best_dist) {
                best_dist = dist;
                best_index = i;
            }
        }

        test_mode_lut[key] = (uint8_t)best_index;
    }
}

/* Fallback ESC detection for test mode:
 * read the ST-written shared key bitmap directly. */
static bool test_mode_esc_held(void) {
    const uint8_t *keys =
        (const uint8_t *)((const uint8_t *)&__rom_in_ram_start__ + ST_KEY_BITMAP_OFFSET);
    const uint8_t esc_mask = (uint8_t)(1u << (TEST_MODE_ESC_SCANCODE & 7));
    return (keys[TEST_MODE_ESC_SCANCODE >> 3] & esc_mask) != 0;
}
#endif

/* ──────────────────────────────────────────────────────────────────────────
 * DMA IRQ handler — same structure as sprites demo
 * ────────────────────────────────────────────────────────────────────────── */
void __not_in_flash_func(emul_dma_irq_handler_lookup)(void) {
    uint32_t m = dma_hw->ints1;

    if (m & (1u << 2)) {
        bool rom3_gpio = (1ul << ROM3_GPIO) & sio_hw->gpio_in;
        uint32_t addr = dma_hw->ch[2].al3_read_addr_trig;
        dma_hw->ints1 = (1u << 2);

        if (!rom3_gpio) {
            uint16_t addr_lsb = (uint16_t)(addr ^ ADDRESS_HIGH_BIT);
            if ((addr_lsb & ST_CMD_KEY_EVENT_MASK) == ST_CMD_KEY_EVENT_BASE) {
                uint8_t scan = (uint8_t)(addr_lsb & ST_CMD_KEY_EVENT_BITS);
                key_event_ttl[scan] = KEY_EVENT_HOLD_FRAMES;
                return;
            }
            switch (addr_lsb) {
                case 0xDCBA:  /* VBLANK tick from ST */
                    sem_release(&draw_sem);
                    break;
                case 0xE1A8:  /* start demo */
                    sem_release(&start_demo_sem);
                    break;
                case 0xABCD:  /* ESC → return to Booster */
                    startBooster = true;
                    sem_release(&draw_sem);
                    break;
                default:
                    break;
            }
        }
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * Core 1 entry: runs rasterizer in a loop, signals Core 0 when done
 * ────────────────────────────────────────────────────────────────────────── */
static semaphore_t raster_done_sem;
static semaphore_t raster_go_sem;

#if !TEST_IMAGE_MODE_ACTIVE
static void __not_in_flash_func(core1_entry)(void) {
    while (1) {
        /* Wait for Core 0 to hand off the triangle list */
        sem_acquire_blocking(&raster_go_sem);

        /* Rasterize triangle list to chunky RGB4444 buffer */
        render_rasterize(number_triangles, chunky_fb);

        /* C2P convert to planar framebuffer */
        c2p_convert_and_double(chunky_fb, (unsigned int *)vga_screen.hidden_framebuffer);

        /* Signal Core 0 that the frame is ready */
        sem_release(&raster_done_sem);
    }
}
#endif

/* ──────────────────────────────────────────────────────────────────────────
 * Helpers
 * ────────────────────────────────────────────────────────────────────────── */
uint32_t emul_getCommandAddress(void) {
    return displayCommandAddress;
}

void emul_preinit(void) {
    displayCommandAddress = (uint32_t)&__rom_in_ram_start__ + CUSTOM_DISPLAY_COMMAND;
    memorySharedAddress   = (uint32_t)&__rom_in_ram_start__;

    SET_SHARED_VAR(TERM_HARDWARE_TYPE,    0, memorySharedAddress, TERM_SHARED_VARIABLES_OFFSET);
    SET_SHARED_VAR(TERM_HARDWARE_VERSION, 0, memorySharedAddress, TERM_SHARED_VARIABLES_OFFSET);
}

/* Write 16-color palette to shared memory so the ST can load it */
static void write_palette_to_shared(void) {
    const uint16_t *pal = c2p_get_palette();
    if (pal == NULL) return;
    uint16_t *shared_pal = (uint16_t *)((uint8_t *)&__rom_in_ram_start__ + ST_PALETTE_OFFSET);
    for (int i = 0; i < 16; i++) {
        shared_pal[i] = pal[i];
    }
}

/* Enable/disable ST-side GEMDOS ESC shortcut to CMD_BOOSTER. */
static void set_st_esc_exit_enabled(bool enabled) {
    uint8_t *flag = (uint8_t *)(memorySharedAddress + ST_ESC_EXIT_ENABLE_OFFSET);
    flag[0] = enabled ? 1u : 0u;
}

/* Optional fallback for key state synthesis from GEMDOS queue events.
 * The ST firmware writes true make/break key state into shared memory, so
 * normal gameplay should consume that directly. */
static void update_key_bitmap_from_key_events(void) {
    uint8_t *keys = (uint8_t *)(uintptr_t)(memorySharedAddress + ST_KEY_BITMAP_OFFSET);
    uint8_t merged[16];

    for (int i = 0; i < 16; i++) {
        merged[i] = keys[i];
    }

    for (uint8_t scan = 0; scan < 128; scan++) {
        uint8_t ttl = key_event_ttl[scan];
        if (ttl == 0) {
            continue;
        }
        merged[scan >> 3] |= (uint8_t)(1u << (scan & 7));
        key_event_ttl[scan] = (uint8_t)(ttl - 1);
    }

    for (int i = 0; i < 16; i++) keys[i] = merged[i];
}

/* Request a clean ST handoff on short SELECT press.
 * This avoids resetting RP2040 mid-frame while the ST is still displaying
 * Sidecar framebuffers. */
static void request_booster_exit(void) {
    startBooster = true;
    sem_release(&draw_sem);
}

/* ──────────────────────────────────────────────────────────────────────────
 * emul_start — main entry point (called from main.c after clock/voltage init)
 * ────────────────────────────────────────────────────────────────────────── */
void __not_in_flash_func(emul_start)(void) {

    startBooster = false;
    memset((void *)key_event_ttl, 0, sizeof(key_event_ttl));
    emul_preinit();

    /* Copy ST firmware to ROM_IN_RAM */
    COPY_FIRMWARE_TO_RAM((uint16_t *)target_firmware, target_firmware_length * 2);
    memset((void *)(uintptr_t)(memorySharedAddress + ST_KEY_BITMAP_OFFSET), 0, 16);
    *(volatile uint8_t *)(uintptr_t)(memorySharedAddress + ST_IKBD_SKIP_COUNT_OFFSET) = 0;

    /* Set up ROM emulation and DMA IRQ */
    SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_NOP);
    init_romemul(NULL, emul_dma_irq_handler_lookup, false);

    sem_init(&draw_sem,       0, 1);
    sem_init(&start_demo_sem, 0, 1);
    sem_init(&raster_done_sem, 0, 1);
    sem_init(&raster_go_sem,   0, 1);

    /* Configure SELECT button
     * TEST_IMAGE_MODE: use Core 1 wait loop (no rasterizer on Core 1).
     * Normal mode   : Core 1 is reserved for rasterizer, so poll from Core 0. */
    select_configure();
#if TEST_IMAGE_MODE_ACTIVE
    select_coreWaitPush(request_booster_exit, reset_deviceAndEraseFlash);
#else
    select_setResetCallback(request_booster_exit);
    select_setLongResetCallback(reset_deviceAndEraseFlash);
#endif

    /* ────────────────────────────────────────────────────────────────────
     * VGA init: two 32KB planar framebuffers in ROM_IN_RAM
     * ──────────────────────────────────────────────────────────────────── */
    uint32_t local_fb_a = (uint32_t)&__rom_in_ram_start__ + 0x10000 - 32000;
    uint32_t local_fb_b = local_fb_a - 32000;

    /* Remote addresses seen from the ST (ROM3 space mapped at 0xFB0000) */
    uint32_t remote_fb_a = 0xFB0000 - 64000;
    uint32_t remote_fb_b = 0xFB0000 - 32000;

    /* Copy-code lives in the region just above shared variable block */
    uint32_t local_copycode_a = (uint32_t)&__rom_in_ram_start__ + 0x600;
    uint32_t local_copycode_b = local_copycode_a + 0x2000;

    if (vga_init(&vga_mode_320x200, local_fb_a, local_fb_b) < 0) {
        DPRINTF("ERROR: VGA init failed\n");
        while (1) sleep_ms(SLEEP_LOOP_MS);
    }
    DPRINTF("VGA initialized\n");

    /* Generate MOVEM.L copy-code for both framebuffers */
    vga_copy_to_display(remote_fb_a, (void *)local_copycode_a,
                        0x70000 /* ST VRAM A, 512KB ST */);
    vga_copy_to_display(remote_fb_b, (void *)local_copycode_b,
                        0x78000 /* ST VRAM B, 512KB ST */);
    DPRINTF("Copy-code generated\n");

    /* ────────────────────────────────────────────────────────────────────
     * Copy font to RAM and initialise font system
     * ──────────────────────────────────────────────────────────────────── */
    memcpy(font6x8_data_ram, font6x8_data_flash, sizeof(font6x8_data_flash));
    font6x8_ram      = font6x8;
    font6x8_ram.data = font6x8_data_ram;
    font_set_font(&font6x8_ram);
    font_set_color(15);

    /* Set initial palette */
#if TEST_IMAGE_MODE_ACTIVE
    build_test_mode_palette_and_lut();
    c2p_set_lut(test_mode_lut, test_mode_palette_st);
#else
    c2p_set_lut(luts[0], palettes[0]);
    last_daylight = 0;
#endif

    /* Set key bitmap address for input system */
    input_set_key_bitmap_address(
        (const void *)((uint8_t *)&__rom_in_ram_start__ + ST_KEY_BITMAP_OFFSET));

#if TEST_IMAGE_MODE_ACTIVE
    set_st_esc_exit_enabled(true);
#else
    set_st_esc_exit_enabled(true);
#endif

    /* ────────────────────────────────────────────────────────────────────
     * Initialise pixel masks (needed by C2P, must be before Core 1 starts)
     * ──────────────────────────────────────────────────────────────────── */
    init_pixel_masks();
    DPRINTF("Pixel masks initialised\n");

    /* ────────────────────────────────────────────────────────────────────
     * Wait for ST to signal demo start (0xE1A8 on ROM3)
     * ──────────────────────────────────────────────────────────────────── */
    DPRINTF("Waiting for ST start signal...\n");
    sem_acquire_blocking(&start_demo_sem);
    DPRINTF("ST ready\n");

#if TEST_IMAGE_MODE_ACTIVE
    DPRINTF("TEST_IMAGE_MODE enabled: rendering static test image\n");

    /* Fill both framebuffers with the same static image using existing C2P path. */
    c2p_convert_and_double(test_mode_image_rgb4444,
                           (unsigned int *)vga_screen.hidden_framebuffer);
    vga_swap_framebuffers();
    c2p_convert_and_double(test_mode_image_rgb4444,
                           (unsigned int *)vga_screen.hidden_framebuffer);
    vga_swap_framebuffers();

    write_palette_to_shared();
    WRITE_AND_SWAP_LONGWORD((uint32_t)&__rom_in_ram_start__,
                            CUSTOM_FRAMEBUFFER_INDEX,
                            vga_screen.current_framebuffer_id);

    DPRINTF("Entering TEST_IMAGE_MODE loop\n");
    while (1) {
        sem_acquire_blocking(&draw_sem);
        update_key_bitmap_from_key_events();
        if (startBooster || test_mode_esc_held()) break;
        write_palette_to_shared();
    }
#else
    /* ────────────────────────────────────────────────────────────────────
     * Start Core 1 rasterizer
     * ──────────────────────────────────────────────────────────────────── */
    multicore_launch_core1(core1_entry);
    DPRINTF("Core 1 launched\n");

    /* Initialise game state */
    logic_new_game();
    menu = MENU_START;
    render_view_projection();

    /* Show both buffers so the ST starts with a valid image */
    vga_swap_framebuffers();
    WRITE_AND_SWAP_LONGWORD((uint32_t)&__rom_in_ram_start__,
                            CUSTOM_FRAMEBUFFER_INDEX,
                            vga_screen.current_framebuffer_id);

    DPRINTF("Entering main loop\n");

    /* ────────────────────────────────────────────────────────────────────
     * Main game loop — driven by ST VBLANK at 50 Hz
     * ──────────────────────────────────────────────────────────────────── */
    while (1) {
        /* Block until ST VBLANK signal (sem_release in DMA IRQ) */
        sem_acquire_blocking(&draw_sem);

        /* In normal mode Core 1 is the rasterizer, so SELECT is polled on Core 0. */
        select_checkPushReset();

        if (startBooster) break;

        /* ── Core 0: logic + triangle building ── */
        global_time++;
        /* Keep GEMDOS key-event fallback merged so input still works if
         * ST-side IKBD bitmap updates are unavailable on some machines/TOS. */
        update_key_bitmap_from_key_events();
        logic_day_night_cycle();
        logic_input();
        logic_events();
        logic_player_area();
        logic_npc();
        logic_zombies();
        logic_grass();

        /* Build triangle list for this frame */
        number_triangles = 0;

        /* Swap triangle lists: Core 1 will read triangle_list_current */
        struct triangle_16 *tmp   = triangle_list_current;
        triangle_list_current     = triangle_list_next;
        triangle_list_next        = tmp;

        render_view_projection();
        render_chunks();

        /* Switch palette if daylight phase changed */
        if (daylight != last_daylight) {
            c2p_set_lut(luts[daylight], palettes[daylight]);
            last_daylight = daylight;
        }

        /* Kick Core 1 to rasterize + C2P */
        sem_release(&raster_go_sem);

        /* While Core 1 rasterizes, Core 0 updates the palette in shared mem */
        write_palette_to_shared();

        /* Wait for Core 1 to finish rendering to the hidden planar buffer */
        sem_acquire_blocking(&raster_done_sem);

        /* Render text overlay (font draws directly to the hidden planar buffer) */
        display_menu();
        display_info();

        /* Swap framebuffers and tell the ST which one to display */
        vga_swap_framebuffers();
        WRITE_AND_SWAP_LONGWORD((uint32_t)&__rom_in_ram_start__,
                                CUSTOM_FRAMEBUFFER_INDEX,
                                vga_screen.current_framebuffer_id);
    }
#endif

    /* ────────────────────────────────────────────────────────────────────
     * Exit to Booster
     * ──────────────────────────────────────────────────────────────────── */
    DPRINTF("Exiting to Booster\n");
    select_setResetCallback(NULL);
    select_setLongResetCallback(NULL);
    select_coreWaitPushDisable();
    set_st_esc_exit_enabled(false);

    /* ST firmware reset handler keys off REMOTE_RESET (command 1). */
    SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_RESET);
    sleep_ms(SLEEP_LOOP_MS);

    deinit_romemul();

    reset_jump_to_booster();
    while (1) sleep_ms(SLEEP_LOOP_MS);
}
