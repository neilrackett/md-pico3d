/* Per-vertex dynamic lighting: collects light data from chunks in a 3x3 grid */
#include "render_globals.h"
#include "render_math.h"
#include "chunk_globals.h"

#include "../chunk_data.h"

int8_t light_falloff = 0;

static int32_t process_lighting(const struct light *light, struct vertex_32 *vertex, color_t *color) {

    if (((light->position.x - vertex->x) * (light->position.x - vertex->x)) +
        ((light->position.y - vertex->y) * (light->position.y - vertex->y)) +
        ((light->position.z - vertex->z) * (light->position.z - vertex->z)) < LIGHT_DISTANCE) {

        uint8_t r = *color & 0x000F;
        uint8_t b = (*color >> 8) & 0x000F;
        uint8_t g = (*color >> 12) & 0x000F;

        b -= light_falloff;

        if (b > 15)
            b = 0;

        *color = g;
        *color <<= 4;
        *color |= b;
        *color <<= 8;
        *color |= r;
        return 1;

    } else {
        return 0;
    }
}

static color_t darken(color_t color) {

    uint8_t r = color & 0x000F;
    uint8_t b = (color >> 8) & 0x000F;
    uint8_t g = (color >> 12) & 0x000F;

    r -= light_falloff;
    b -= light_falloff;
    g -= light_falloff;

    if (r > 15) r = 0;
    if (b > 15) b = 0;
    if (g > 15) g = 0;

    color = g;
    color <<= 4;
    color |= b;
    color <<= 8;
    color |= r;

    return color;
}

static void vertex_lighting(struct vertex_32 *in, color_t *color, int16_t chunk_x, int16_t chunk_y) {

    for (int i = 0; i < chunk_lights[chunk_x][chunk_y].number; i++) {
        if (process_lighting(&chunk_lights[chunk_x][chunk_y].lights[i], in, color) == 1) {
            return;
        }
    }

    for (int x = -1; x < 2; x++) {
        int32_t newx = chunk_x + x;

        if (newx < 0 || newx >= WORLD_SIZE_X)
            continue;

        for (int y = -1; y < 2; y++) {
            int32_t newy = chunk_y + y;

            if (newy < 0 || newy >= WORLD_SIZE_Y)
                continue;

            if (!(x == 0 && y == 0)) {
                for (int i = 0; i < chunk_lights[newx][newy].number; i++) {
                    if (process_lighting(&chunk_lights[newx][newy].lights[i], in, color) == 1) {
                        return;
                    }
                }
            }
        }
    }

    *color = darken(*color);
}

void render_lighting(struct triangle_32 *in) {

    if (light_falloff == 0)
        return;

    if (in->shader_id > 10)
        return;

    int32_t chunk_x;
    int32_t chunk_y;

#ifndef NO_GLOBAL_OFFSET
    in->vertex1.x += global_offset_x * CHUNK_SIZE;
    in->vertex2.x += global_offset_x * CHUNK_SIZE;
    in->vertex3.x += global_offset_x * CHUNK_SIZE;

    in->vertex1.z += global_offset_z * CHUNK_SIZE;
    in->vertex2.z += global_offset_z * CHUNK_SIZE;
    in->vertex3.z += global_offset_z * CHUNK_SIZE;
#endif

    chunk_locate(in->vertex1.x, in->vertex1.z, &chunk_x, &chunk_y);

    if (chunk_x == -1 || chunk_y == -1)
        return;

    vertex_lighting(&in->vertex1, &in->vertex_parameter1.color, chunk_x, chunk_y);
    vertex_lighting(&in->vertex2, &in->vertex_parameter2.color, chunk_x, chunk_y);
    vertex_lighting(&in->vertex3, &in->vertex_parameter3.color, chunk_x, chunk_y);

    if (in->vertex_parameter1.color == in->vertex_parameter2.color &&
        in->vertex_parameter2.color == in->vertex_parameter3.color) {
        in->shader_id = 1;
    } else {
        in->shader_id = 2;
    }
}
