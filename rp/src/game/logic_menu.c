/* Display and handle menus (main, start screen, death) */
#include "logic_globals.h"
#include "../engine/render_globals.h"
#include "../include/vga/font.h"

int32_t menu = MENU_START;

#ifdef BENCHMARK
extern int32_t benchmark_complete;
#endif

#ifdef FRAME_COUNTER
extern uint32_t perf_25_below;
extern uint32_t perf_50_below;
extern uint32_t perf_75_below;
extern uint32_t perf_75_above;
#endif

#ifdef DEBUG_INFO
extern int32_t logic_time;
#endif

void display_menu(void) {

    font_set_color(15);

    if (menu == MENU_MAIN) {

        font_move(0, 0);   font_print("MENU:");
        font_move(0, 20*2); font_printf("Health: %d", (int)player_health);
        font_move(0, 30*2); font_printf("Ammo: %d",   (int)player_ammo);
        font_move(0, 40*2); font_printf("Kills: %d",  (int)player_kills);
        font_move(0, 50*2); font_printf("Money: %d$", (int)player_money);

#ifdef FRAME_COUNTER
        font_move(60*2,  0);   font_printf("40:%d", (int)perf_25_below);
        font_move(60*2, 10*2); font_printf("20:%d", (int)perf_50_below);
        font_move(60*2, 20*2); font_printf("13:%d", (int)perf_75_below);
        font_move(60*2, 30*2); font_printf("<13:%d",(int)perf_75_above);
#endif

#ifdef DEBUG_INFO
        font_move(0,   70*2); font_printf("#R: %d", (int)rendered_triangles);
        font_move(60*2,70*2); font_printf("#C: %d", (int)cached_triangles);
        font_move(0,   80*2); font_printf("C0U:%d", (int)logic_time);
#endif

        font_move(0, 100*2); font_print("UP/DOWN: (no brightness on ST)");
        font_move(0, 110*2); font_print("ESC: Exit");

    } else if (menu == MENU_START) {

#ifdef BENCHMARK
        if (benchmark_complete == 0) {
            font_move(0, 0); font_print("BENCHMARKING");
        }
#else
        if (demo_progress < 2500) {
            font_move(28*2, 20*2); font_print("Pico3D Engine");
        }

        if ((demo_progress / 32) % 2 == 0) {
            font_move(20*2, 90*2); font_print("Press any key");
        }
#endif

    } else if (menu == MENU_DEATH) {
        font_move(38*2, 20*2); font_print("YOU DIED!");
    }
}
