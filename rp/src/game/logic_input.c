/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Handle ST keyboard input via shared memory bitmap */
#include "logic_globals.h"
#include "../engine/render_globals.h"
#include "../engine/render_math.h"
#include "../engine/chunk_globals.h"

/* Atari ST IKBD scancodes */
#define KEY_UP      0x48
#define KEY_DOWN    0x50
#define KEY_LEFT    0x4B
#define KEY_RIGHT   0x4D
#define KEY_SPACE   0x39  /* Shoot / talk */
#define KEY_A       0x1E  /* Look up */
#define KEY_Z       0x2C  /* Look down */
#define KEY_M       0x32  /* Menu toggle */
#define KEY_H       0x23  /* Help fallback toggle */
#define KEY_F1      0x3B  /* Help toggle */
#define KEY_ESCAPE  0x01  /* Always exit to Booster (handled by ST firmware) */

/* Keep translation movement speed unchanged, but reduce yaw speed by 90%
 * for smoother turning on keyboard input. */
#define TURN_SENSITIVITY (INPUT_SENSITIVITY * 0.3f)
#define LOOK_SENSITIVITY TURN_SENSITIVITY

/* Key state bitmap: 128 bits (16 bytes), one bit per scancode 0-127.
 * Populated by the ST assembly each frame from ACIA keyboard polling. */
static uint8_t key_state[16];
static uint8_t key_state_prev[16];

/* Address in ROM_IN_RAM where the ST writes the key bitmap.
 * Must match the address used in the ST assembly. Set by emul.c at startup. */
static const uint8_t *st_key_bitmap = NULL;

void input_set_key_bitmap_address(const void *addr) {
    st_key_bitmap = (const uint8_t *)addr;
}

void input_update(void) {
    /* Save previous state for edge detection */
    for (int i = 0; i < 16; i++) {
        key_state_prev[i] = key_state[i];
    }
    /* Read current state from shared memory */
    if (st_key_bitmap != NULL) {
        for (int i = 0; i < 16; i++) {
            key_state[i] = st_key_bitmap[i];
        }
    }
}

/* Returns non-zero if key (scancode 0-127) is currently held */
static int key_held(uint8_t scancode) {
    if (scancode >= 128) return 0;
    return (key_state[scancode >> 3] >> (scancode & 7)) & 1;
}

/* Returns non-zero if key was just pressed this frame */
static int key_pressed(uint8_t scancode) {
    if (scancode >= 128) return 0;
    int cur  = (key_state     [scancode >> 3] >> (scancode & 7)) & 1;
    int prev = (key_state_prev[scancode >> 3] >> (scancode & 7)) & 1;
    return cur && !prev;
}

/* Returns non-zero if any key transitioned from up->down this frame. */
static int any_key_pressed(void) {
    for (int i = 0; i < 16; i++) {
        if ((key_state[i] & (uint8_t)~key_state_prev[i]) != 0) {
            return 1;
        }
    }
    return 0;
}

static void try_move_camera(float move) {
    float old_cam_x = camera_position[0];
    float old_cam_z = camera_position[2];

    move_camera(move);

#ifndef FREE_ROAM
    if (!chunk_traversable(float_to_int(camera_position[0]), float_to_int(camera_position[2]), 0)) {
        camera_position[0] = old_cam_x;
        camera_position[2] = old_cam_z;
    }
#endif
}

void logic_input(void) {

    input_update();

    if (menu == 0) {

        if (key_held(KEY_LEFT)) {
            yaw += TURN_SENSITIVITY;
            update_camera();
        }

        if (key_held(KEY_RIGHT)) {
            yaw -= TURN_SENSITIVITY;
            update_camera();
        }

        if (key_held(KEY_UP)) {
            try_move_camera(INPUT_SENSITIVITY);
            update_camera();
        }

        if (key_held(KEY_DOWN)) {
            try_move_camera(-INPUT_SENSITIVITY);
            update_camera();
        }

        if (key_held(KEY_A)) {
            pitch += LOOK_SENSITIVITY;
            update_camera();
        }

        if (key_held(KEY_Z)) {
            pitch -= LOOK_SENSITIVITY;
            update_camera();
        }

        if (key_pressed(KEY_SPACE)) {
            if (player_area == AREA_OUTSKIRTS) {
                logic_shoot();
            }
            if (close_npc != -1) {
                talk_quest_npc();
            }
        }

        if (key_pressed(KEY_M)) {
            menu = MENU_MAIN;
        }

        if (key_pressed(KEY_F1) || key_pressed(KEY_H)) {
            logic_toggle_help_overlay();
        }

    } else if (menu == MENU_MAIN) {

        if (key_pressed(KEY_M)) {
            menu = 0;
        }

#ifdef FREE_ROAM
        if (key_held(KEY_A)) {
            camera_position[1] += 0.1f;
            update_camera();
        }
        if (key_held(KEY_Z)) {
            camera_position[1] -= 0.1f;
            update_camera();
        }
#endif

#ifdef DEBUG_SHADERS
        if (key_pressed(KEY_LEFT)) {
            if (shader_override < 250) {
                shader_override = 250;
            } else if (shader_override >= 254) {
                shader_override = 0;
            } else {
                shader_override++;
            }
        }
#endif

    } else if (menu == MENU_START) {

#ifndef BENCHMARK
        if (any_key_pressed()) {
            logic_new_game();
            menu = 0;
        }
#endif

    } else if (menu == MENU_DEATH) {
        /* nothing */
    }
}
