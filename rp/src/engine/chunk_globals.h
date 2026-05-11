/* Chunk cache globals and world geometry definitions */

#ifndef CHUNK_GLOBALS_H
#define CHUNK_GLOBALS_H

#include "render_globals.h"
#include "render_math.h"

/* World chunk dimensions defined by the Blender exporter in chunk_data.h */
/* #define WORLD_SIZE_X 12 */
/* #define WORLD_SIZE_Y 12 */

#define CHUNK_UNITS 10  /* 10 metre size per chunk (max ~64) */

#define CHUNK_SIZE (FIXED_POINT_FACTOR * CHUNK_UNITS)

/* Starting position of chunks in world coordinates */
#define CHUNK_OFFSET_X (-(WORLD_SIZE_X * CHUNK_SIZE) / 2)
#define CHUNK_OFFSET_Y (-(WORLD_SIZE_Y * CHUNK_SIZE) / 2)

/* Chunk cache settings */
#define LOD0_GRID_WIDTH  3  /* inner 3x3 highest-detail (odd numbers only) */
#define LOD1_GRID_WIDTH  2  /* outer shell lower-detail (even numbers) */
#define GRID_WIDTH       ((LOD1_GRID_WIDTH * 2) + LOD0_GRID_WIDTH)
#define TOTAL_CHUNKS     (GRID_WIDTH * GRID_WIDTH)

#define MAX_CHUNK_CACHE_TRIANGLES 1800

extern int32_t cached_triangles;

struct chunk {
    uint16_t num_triangles;
    uint16_t reserved;
    const struct triangle_16 *triangles;
};

extern struct triangle_16 chunk_cache[MAX_CHUNK_CACHE_TRIANGLES];

/* chunk_locate: returns chunk grid coordinates for a world position.
 * chunk_x and chunk_y are output parameters (set to -1 if out of bounds). */
void chunk_locate(int32_t x, int32_t y, int32_t *chunk_x, int32_t *chunk_y);

void render_chunks(void);

#endif /* CHUNK_GLOBALS_H */
