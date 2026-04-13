/* Chunk loading: fills the chunk cache from flash geometry data */
#include "render_globals.h"
#include "render_math.h"
#include "chunk_globals.h"

#include "../chunk_data.h"

int32_t cached_triangles = 0;

static int32_t current_chunk_x = -1;
static int32_t current_chunk_y = -1;

struct triangle_16 chunk_cache[MAX_CHUNK_CACHE_TRIANGLES];

/* Locate the chunk grid cell for a world position (fixed-point).
 * Sets *chunk_x and *chunk_y to -1 if position is out of bounds. */
void chunk_locate(int32_t x, int32_t y, int32_t *chunk_x, int32_t *chunk_y) {

    if (x < CHUNK_OFFSET_Y) {
        *chunk_y = -1;
    } else {
        *chunk_y = (x - CHUNK_OFFSET_Y) / CHUNK_SIZE;
        if (*chunk_y >= WORLD_SIZE_Y)
            *chunk_y = -1;
    }

    if (y < CHUNK_OFFSET_X) {
        *chunk_x = -1;
    } else {
        *chunk_x = (y - CHUNK_OFFSET_X) / CHUNK_SIZE;
        if (*chunk_x >= WORLD_SIZE_X)
            *chunk_x = -1;
    }
}

static void load_lod0_chunk(int32_t x, int32_t y) {

    int32_t num_triangles = lod0_chunks[x][y].num_triangles;

    if (cached_triangles + num_triangles >= MAX_CHUNK_CACHE_TRIANGLES)
        return;

    for (int i = 0; i < num_triangles; i++) {
        chunk_cache[cached_triangles].vertex1.x = lod0_chunks[x][y].triangles[i].vertex1.x;
        chunk_cache[cached_triangles].vertex1.y = lod0_chunks[x][y].triangles[i].vertex1.y;
        chunk_cache[cached_triangles].vertex1.z = lod0_chunks[x][y].triangles[i].vertex1.z;

        chunk_cache[cached_triangles].vertex2.x = lod0_chunks[x][y].triangles[i].vertex2.x;
        chunk_cache[cached_triangles].vertex2.y = lod0_chunks[x][y].triangles[i].vertex2.y;
        chunk_cache[cached_triangles].vertex2.z = lod0_chunks[x][y].triangles[i].vertex2.z;

        chunk_cache[cached_triangles].vertex3.x = lod0_chunks[x][y].triangles[i].vertex3.x;
        chunk_cache[cached_triangles].vertex3.y = lod0_chunks[x][y].triangles[i].vertex3.y;
        chunk_cache[cached_triangles].vertex3.z = lod0_chunks[x][y].triangles[i].vertex3.z;

        chunk_cache[cached_triangles].shader_id  = lod0_chunks[x][y].triangles[i].shader_id;
        chunk_cache[cached_triangles].texture_id = lod0_chunks[x][y].triangles[i].texture_id;

        chunk_cache[cached_triangles].vertex_parameter1.color = lod0_chunks[x][y].triangles[i].vertex_parameter1.color;
        chunk_cache[cached_triangles].vertex_parameter2.color = lod0_chunks[x][y].triangles[i].vertex_parameter2.color;
        chunk_cache[cached_triangles].vertex_parameter3.color = lod0_chunks[x][y].triangles[i].vertex_parameter3.color;

        chunk_cache[cached_triangles].chunk_x = x;
        chunk_cache[cached_triangles].chunk_y = y;

        cached_triangles++;
    }
}

static void load_lod1_chunk(int32_t x, int32_t y) {

    int32_t num_triangles = lod1_chunks[x][y].num_triangles;

    if (cached_triangles + num_triangles >= MAX_CHUNK_CACHE_TRIANGLES)
        return;

    for (int i = 0; i < num_triangles; i++) {
        chunk_cache[cached_triangles].vertex1.x = lod1_chunks[x][y].triangles[i].vertex1.x;
        chunk_cache[cached_triangles].vertex1.y = lod1_chunks[x][y].triangles[i].vertex1.y;
        chunk_cache[cached_triangles].vertex1.z = lod1_chunks[x][y].triangles[i].vertex1.z;

        chunk_cache[cached_triangles].vertex2.x = lod1_chunks[x][y].triangles[i].vertex2.x;
        chunk_cache[cached_triangles].vertex2.y = lod1_chunks[x][y].triangles[i].vertex2.y;
        chunk_cache[cached_triangles].vertex2.z = lod1_chunks[x][y].triangles[i].vertex2.z;

        chunk_cache[cached_triangles].vertex3.x = lod1_chunks[x][y].triangles[i].vertex3.x;
        chunk_cache[cached_triangles].vertex3.y = lod1_chunks[x][y].triangles[i].vertex3.y;
        chunk_cache[cached_triangles].vertex3.z = lod1_chunks[x][y].triangles[i].vertex3.z;

        chunk_cache[cached_triangles].shader_id  = lod1_chunks[x][y].triangles[i].shader_id;
        chunk_cache[cached_triangles].texture_id = lod1_chunks[x][y].triangles[i].texture_id;

        chunk_cache[cached_triangles].vertex_parameter1.color = lod1_chunks[x][y].triangles[i].vertex_parameter1.color;
        chunk_cache[cached_triangles].vertex_parameter2.color = lod1_chunks[x][y].triangles[i].vertex_parameter2.color;
        chunk_cache[cached_triangles].vertex_parameter3.color = lod1_chunks[x][y].triangles[i].vertex_parameter3.color;

        chunk_cache[cached_triangles].chunk_x = x;
        chunk_cache[cached_triangles].chunk_y = y;

        cached_triangles++;
    }
}

static void chunk_cache_fill(void) {

    int32_t chunk_x, chunk_y;
    cached_triangles = 0;

    chunk_locate(camera_position_fixed_point[0], camera_position_fixed_point[2], &chunk_x, &chunk_y);

    if (chunk_x == -1 || chunk_y == -1)
        return;

    load_lod0_chunk(chunk_x, chunk_y);

    int32_t x = chunk_x - (LOD0_GRID_WIDTH / 2);

    for (int i = 0; i < LOD0_GRID_WIDTH; i++) {
        int32_t y = chunk_y - (LOD0_GRID_WIDTH / 2);
        for (int j = 0; j < LOD0_GRID_WIDTH; j++) {
            if (x >= 0 && x < WORLD_SIZE_X && y >= 0 && y < WORLD_SIZE_Y) {
                if (!(x == chunk_x && y == chunk_y)) {
                    load_lod0_chunk(x, y);
                }
            }
            y++;
        }
        x++;
    }

    if (cached_triangles >= MAX_CHUNK_CACHE_TRIANGLES)
        return;

    x = chunk_x - (GRID_WIDTH / 2);

    for (int i = 0; i < GRID_WIDTH; i++) {
        int32_t y = chunk_y - (GRID_WIDTH / 2);
        for (int j = 0; j < GRID_WIDTH; j++) {
            if ((i < LOD1_GRID_WIDTH || i >= LOD1_GRID_WIDTH + LOD0_GRID_WIDTH) ||
                (j < LOD1_GRID_WIDTH || j >= LOD1_GRID_WIDTH + LOD0_GRID_WIDTH)) {
                if (x >= 0 && x < WORLD_SIZE_X && y >= 0 && y < WORLD_SIZE_Y) {
                    load_lod1_chunk(x, y);
                }
            }
            y++;
        }
        x++;
    }
}

void render_chunks(void) {

    int32_t chunk_x, chunk_y;

    chunk_locate(camera_position_fixed_point[0], camera_position_fixed_point[2], &chunk_x, &chunk_y);

    if (chunk_x != current_chunk_x || chunk_y != current_chunk_y) {
        current_chunk_x = chunk_x;
        current_chunk_y = chunk_y;
        chunk_cache_fill();
    }

    if (skip_frame == 0) {
        int16_t old_x = -1;
        int16_t old_y = -1;
        int32_t offset_x = 0;
        int32_t offset_y = 0;

        for (int i = 0; i < cached_triangles; i++) {

            if (old_x != chunk_cache[i].chunk_x || old_y != chunk_cache[i].chunk_y) {
                offset_x = (chunk_cache[i].chunk_x * CHUNK_SIZE) + CHUNK_OFFSET_X + (CHUNK_SIZE / 2);
                offset_y = (chunk_cache[i].chunk_y * CHUNK_SIZE) + CHUNK_OFFSET_Y + (CHUNK_SIZE / 2);
                old_x = chunk_cache[i].chunk_x;
                old_y = chunk_cache[i].chunk_y;
            }

            struct triangle_32 new_triangle;

            new_triangle.vertex1.x = chunk_cache[i].vertex1.x + offset_y;
            new_triangle.vertex1.y = chunk_cache[i].vertex1.y;
            new_triangle.vertex1.z = chunk_cache[i].vertex1.z + offset_x;

            new_triangle.vertex2.x = chunk_cache[i].vertex2.x + offset_y;
            new_triangle.vertex2.y = chunk_cache[i].vertex2.y;
            new_triangle.vertex2.z = chunk_cache[i].vertex2.z + offset_x;

            new_triangle.vertex3.x = chunk_cache[i].vertex3.x + offset_y;
            new_triangle.vertex3.y = chunk_cache[i].vertex3.y;
            new_triangle.vertex3.z = chunk_cache[i].vertex3.z + offset_x;

            new_triangle.shader_id  = chunk_cache[i].shader_id;
            new_triangle.texture_id = chunk_cache[i].texture_id;

            new_triangle.vertex_parameter1.color = chunk_cache[i].vertex_parameter1.color;
            new_triangle.vertex_parameter2.color = chunk_cache[i].vertex_parameter2.color;
            new_triangle.vertex_parameter3.color = chunk_cache[i].vertex_parameter3.color;

            render_triangle(&new_triangle);

            if (number_triangles == MAX_RENDER_TRIANGLES)
                break;
        }
    }
}
