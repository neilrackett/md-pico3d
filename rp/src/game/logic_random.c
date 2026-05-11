/* Simple pseudorandom number generator */
#include "logic_globals.h"

static int32_t last_rand = 0;

int32_t rand_range(int32_t rand_min, int32_t rand_max) {
    int32_t rand = (global_time + last_rand) % ((rand_max + 1) - rand_min);
    last_rand = rand;
    return rand_min + rand;
}
