/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LOGIC_GLOBALS_H
#define LOGIC_GLOBALS_H

#include <stdint.h>
#include "../engine/render_globals.h"

extern uint32_t global_time;
extern int8_t daylight;

#define QUARTER_DAY 16384  /* duration of a quarter day in ms */

#define DAY_R   13
#define DAY_G   14
#define DAY_B   15

#define NIGHT_R  2
#define NIGHT_G  1
#define NIGHT_B  8

/* Timeout game if no input detected at gamescom */
#ifdef GAMESCOM
extern int32_t input_idle_timer;
#define INPUT_TIMEOUT 2500
#endif

/* Game areas */
#define AREA_OUTSKIRTS        1
#define AREA_CITY_CENTER      2
#define AREA_YAKUZA_ALLEY     3
#define AREA_DOWNTOWN         4
#define AREA_OUTSKIRT_STABLES 5
#define SHOTGUN_DAMAGE        20
extern int32_t player_area;

/* Player properties */
extern int32_t player_health;
extern int32_t player_ammo;
extern int32_t player_kills;
extern int32_t player_money;

/* Gameplay menus */
#define MENU_START  1
#define MENU_MAIN   2
#define MENU_SHOP   3
#define MENU_HOTEL  4
#define MENU_DEATH  5
#define DEATH_DURATION 400
extern int32_t menu;
extern int32_t demo_progress;

/* NPC logic */
#ifdef NO_NPCS
#define MAX_NPCS 0
#else
#define MAX_NPCS 50
#endif
#define NPC_TRIANGLE_BUDGET   800
#define NPC_SPEED             30
#define NPC_VIEW_DISTANCE     (FIXED_POINT_FACTOR * 20)
#define NPC_DESTROY_DISTANCE  (FIXED_POINT_FACTOR * 50)
#define NPC_WALK_TIME         128

/* Quest NPCs */
#define MAX_QUEST_NPCS        3
#define QUEST_NPC_TALK_DISTANCE (FIXED_POINT_FACTOR * 3)
extern int32_t close_npc;

#define DIALOGUE_TIMER        200
extern int32_t dialogue_time_remain;
extern int32_t dialogue_display;

#define QUEST_AMMO_COST       10
#define QUEST_AMMO_PURCHASE   5
#define QUEST_KILL_REWARD     3

/* Zombie logic */
#ifdef NO_NPCS
#define MAX_ZOMBIES 0
#else
#define MAX_ZOMBIES 50
#endif
#define ZOMBIE_TRIANGLE_BUDGET      800
#define ZOMBIE_HEALTH               100
#define ZOMBIE_WALK_SPEED           30
#define ZOMBIE_RUN_SPEED            60
#define ZOMBIE_VIEW_DISTANCE        (FIXED_POINT_FACTOR * 20)
#define ZOMBIE_SHOOT_DISTANCE       (FIXED_POINT_FACTOR * 5)
#define ZOMBIE_ATTACK_DISTANCE      ((int32_t)(FIXED_POINT_FACTOR * 1.5f))
#define ZOMBIE_DESTROY_DISTANCE     (FIXED_POINT_FACTOR * 50)
#define ZOMBIE_TRACK_DISTANCE_DAY   (FIXED_POINT_FACTOR * 10)
#define ZOMBIE_TRACK_DISTANCE_NIGHT (FIXED_POINT_FACTOR * 20)
#define ZOMBIE_WALK_TIME            128
#define ZOMBIE_RUN_TIME             64
#define ZOMBIE_ATTACK_DURATION      8
#define ZOMBIE_SLOUCH_DURATION      32
#define ZOMBIE_DEATH_DURATION       32
#define ZOMBIE_DESPAWN_DURATION     1024
#define ZOMBIE_ATTACK_DAMAGE        1

/* Input */
#define INPUT_SENSITIVITY 0.1f

/* Chunk-based physics */
#define CHUNK_BORDER (FIXED_POINT_FACTOR * 1)
extern uint8_t chunk_physics[12][12];
uint8_t chunk_traversable(int32_t x, int32_t y, uint8_t character_type);

struct npc {
    int8_t  status;       /* -1 = unused */
    int8_t  direction;
    int16_t progress;
    color_t shirt_color;
    int8_t  health;
    int8_t  dialogue;
    int32_t x;
    int32_t y;
};

extern struct npc npc_list[MAX_NPCS];
extern struct npc npc_quest_list[MAX_QUEST_NPCS];
extern struct npc zombie_list[MAX_ZOMBIES];

int32_t rand_range(int32_t rand_min, int32_t rand_max);

void logic_day_night_cycle(void);
void logic_demo(void);
void logic_events(void);
void logic_grass(void);
void logic_input(void);
void logic_new_game(void);
void logic_npc(void);
void logic_player_area(void);
void logic_toggle_help_overlay(void);
void logic_shoot(void);
void logic_zombies(void);

void render_gate(void);
void render_grass(void);
void display_info(void);
void display_menu(void);
void render_npcs(void);

void init_quest_npcs(void);
void talk_quest_npc(void);
void render_quest_npcs(void);
void render_zombies(void);

#endif /* LOGIC_GLOBALS_H */
