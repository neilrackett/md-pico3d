/* HUD overlay: health/ammo display, area name, dialogue */
#include "logic_globals.h"
#include "../engine/render_globals.h"
#include "../engine/chunk_globals.h"
#include "../include/vga/font.h"

#define INFO_TIMER 120

int32_t player_area = 0;

int32_t info_time_remain  = 0;
int32_t info_display      = 0;

int32_t dialogue_time_remain = 0;
int32_t dialogue_display     = 0;
static int32_t help_overlay_visible = 0;

void logic_toggle_help_overlay(void) {
    help_overlay_visible = !help_overlay_visible;
}

void logic_player_area(void) {

    int32_t chunk_x, chunk_y;
    chunk_locate(camera_position_fixed_point[0], camera_position_fixed_point[2], &chunk_x, &chunk_y);

    int32_t new_area;

    if (chunk_y >= 6) {
        new_area = AREA_OUTSKIRTS;
        if (chunk_y >= 10 && chunk_x < 3) {
            new_area = AREA_OUTSKIRT_STABLES;
        }
    } else if (chunk_x > 7) {
        new_area = AREA_YAKUZA_ALLEY;
    } else if (chunk_x < 5) {
        new_area = AREA_DOWNTOWN;
    } else {
        new_area = AREA_CITY_CENTER;
    }

    if (new_area != player_area) {
        info_time_remain = INFO_TIMER;
        player_area = new_area;
    }
}

void display_info(void) {
    font_set_color(15);

    if (menu == 0) {
        font_move(95*2, 90*2);
        font_print("F1: Help");
    }

    if (player_area == AREA_OUTSKIRTS) {

        /* Health */
        if (player_health < 30) {
            font_set_color(15);  /* would be red, but we only have index 15 as white */
        } else {
            font_set_color(15);
        }
        font_move(0, 0);
        font_printf("+%d", (int)player_health);

        /* Ammo */
        font_set_color(15);
        if (player_ammo == 0) {
            font_move(75 * 2, 0);
            font_print("No Ammo");
        } else if (player_ammo > 99) {
            font_move(100*2, 0);
            font_print_int((int)player_ammo);
        } else if (player_ammo > 9) {
            font_move(107 * 2, 0);
            font_print_int((int)player_ammo);
        } else {
            font_move(114 * 2, 0);
            font_print_int((int)player_ammo);
        }

        /* Crosshair — write directly to planar framebuffer via font pixel ops not available;
         * skip for now, can be implemented in the C2P pass later */
    }

    /* Area name */
    font_set_color(15);
    if (info_time_remain != 0) {
        font_move(20*2, 10*2);
        if (player_area == AREA_OUTSKIRTS) {
            font_print("Outskirts");
        } else if (player_area == AREA_YAKUZA_ALLEY) {
            font_print("Back Alley");
        } else if (player_area == AREA_DOWNTOWN) {
            font_print("Downtown");
        } else if (player_area == AREA_CITY_CENTER) {
            font_print("City Center");
        } else if (player_area == AREA_OUTSKIRT_STABLES) {
            font_print("Outskirt / Stable");
        }
        info_time_remain--;
    }

    /* Talk prompt */
    if (close_npc != -1) {
        if (close_npc == 1 &&
            (npc_quest_list[close_npc].dialogue == 11 || npc_quest_list[close_npc].dialogue == 12)) {
            font_move(0, 90*2); font_print("SPACE: Buy Ammo");
        } else {
            font_move(0, 90*2); font_print("SPACE: Talk");
        }
    }

    /* NPC dialogue */
    if (dialogue_time_remain != 0) {
        switch (dialogue_display) {
            case 0:  font_move(0, 90*2); font_print("Hey there! You seem");
                     font_move(0,95*5); font_print("to be new around here."); break;
            case 1:  font_move(0, 90*2); font_print("I am guarding the");
                     font_move(0,95*5); font_print("city from zombies."); break;
            case 2:  font_move(0, 90*2); font_print("Be careful if you");
                     font_move(0,95*5); font_print("go out, they will attack."); break;
            case 3:  font_move(0, 90*2); font_print("The gates close at");
                     font_move(0,95*5); font_print("night. Be back by then."); break;
            case 4:  font_move(0, 90*2); font_print("Zombies are more");
                     font_move(0,95*5); font_print("aggressive in the dark."); break;
            case 5:  font_move(0, 90*2); font_print("Try not to run out");
                     font_move(0,95*5); font_print("of ammo out there."); break;
            case 6:  if (player_kills <= 100) {
                         font_move(0, 90*2); font_print("See if you can kill");
                         font_move(0,95*5); font_print("a couple of the zombies.");
                     } else {
                         font_move(0, 90*2); font_print("Wow you got over a 100!");
                         font_move(0,95*5); font_print("You're a zombie killer!");
                     }
                     break;
            case 10: font_move(0, 90*2); font_print("Need Bullets?");
                     font_move(0,95*5); font_print("10$ for 5."); break;
            case 11: font_move(0, 90*2); font_print("Good hunting.");
                     font_move(0,95*5); font_printf("%d$ -> %d$",
                         (int)(player_money + QUEST_AMMO_COST), (int)player_money); break;
            case 12: font_move(0, 90*2); font_print("You don't have");
                     font_move(0,95*5); font_print("enough money."); break;
            case 20: font_move(0, 90*2); font_print("Oh, well done for");
                     font_move(0,95*5); font_print("making it here."); break;
            case 21: font_move(0, 90*2); font_print("The fire keeps the");
                     font_move(0,95*5); font_print("zombies away."); break;
            case 22: font_move(0, 90*2); font_print("This place used to");
                     font_move(0,95*5); font_print("have horses but..."); break;
            case 23: font_move(0, 90*2); font_print("I mean look at the");
                     font_move(0,95*5); font_print("surroundings..."); break;
            case 24: font_move(0, 90*2); font_print("The stable owner is");
                     font_move(0,95*5); font_print("pretty unhappy."); break;
            case 25: font_move(0, 90*2); font_print("Sells ammo to anyone");
                     font_move(0,95*5); font_print("hoping it will help."); break;
            case 26: font_move(0, 90*2); font_print("Don't get killed and");
                     font_move(0,95*5); font_print("get some rest here."); break;
            default: break;
        }
        dialogue_time_remain--;
    }

    if (menu == 0 && help_overlay_visible) {
        font_move(20*2, 40*2); font_print("UP / DOWN        Move");
        font_move(20*2, 45*2); font_print("LEFT / RIGHT     Rotate");
        font_move(20*2, 50*2); font_print("A / Z            Look");
        font_move(20*2, 55*2); font_print("SPACE            Shoot / Interact");
        font_move(20*2, 60*2); font_print("M                Menu");
        font_move(20*2, 65*2); font_print("ESC              Exit");
    }
}
