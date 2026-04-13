/* function to transform incoming triangle, perform clipping & lighting and put
 * it in the triangle list if visible. The triangle will then be rasterized by
 * render_rasterize() on Core1 on the next frame. */
#include "render_globals.h"
#include "render_math.h"
#include "chunk_globals.h"

uint32_t number_triangles = 0;

#ifdef DEBUG_INFO
uint32_t rendered_triangles = 0;
#endif

static struct triangle_16 triangle_list1[MAX_RENDER_TRIANGLES];
static struct triangle_16 triangle_list2[MAX_RENDER_TRIANGLES];
struct triangle_16 *triangle_list_current = triangle_list1;
struct triangle_16 *triangle_list_next = triangle_list2;

#define CODE_INSIDE 0
#define CODE_LEFT   1
#define CODE_RIGHT  2
#define CODE_BOTTOM 4
#define CODE_TOP    8

static uint8_t compute_code(int32_t x, int32_t y) {
    uint8_t code = CODE_INSIDE;
    if      (x < 0)            code |= CODE_LEFT;
    else if (x > SCREEN_WIDTH)  code |= CODE_RIGHT;
    if      (y < 0)            code |= CODE_BOTTOM;
    else if (y > SCREEN_HEIGHT) code |= CODE_TOP;
    return code;
}

void render_triangle(struct triangle_32 *in) {

    if (number_triangles >= MAX_RENDER_TRIANGLES) return;

#ifndef NO_GLOBAL_OFFSET
    in->vertex1.x -= global_offset_x * CHUNK_SIZE;
    in->vertex2.x -= global_offset_x * CHUNK_SIZE;
    in->vertex3.x -= global_offset_x * CHUNK_SIZE;
    in->vertex1.z -= global_offset_z * CHUNK_SIZE;
    in->vertex2.z -= global_offset_z * CHUNK_SIZE;
    in->vertex3.z -= global_offset_z * CHUNK_SIZE;
#endif

    int32_t z_near = (int32_t)(ZNEAR * FIXED_POINT_FACTOR * FIXED_POINT_FACTOR);

    struct triangle_32 output_triangle;
    struct triangle_32 extra_triangle;
    int8_t render_extra_triangle = 0;
    uint8_t rearrange_parameters = 0;

    int32_t w1 = ((mat_vp[3][0] * in->vertex1.x) + (mat_vp[3][1] * in->vertex1.y) + (mat_vp[3][2] * in->vertex1.z) + (mat_vp[3][3] * FIXED_POINT_FACTOR)) / FIXED_POINT_FACTOR;
    int32_t w2 = ((mat_vp[3][0] * in->vertex2.x) + (mat_vp[3][1] * in->vertex2.y) + (mat_vp[3][2] * in->vertex2.z) + (mat_vp[3][3] * FIXED_POINT_FACTOR)) / FIXED_POINT_FACTOR;
    int32_t w3 = ((mat_vp[3][0] * in->vertex3.x) + (mat_vp[3][1] * in->vertex3.y) + (mat_vp[3][2] * in->vertex3.z) + (mat_vp[3][3] * FIXED_POINT_FACTOR)) / FIXED_POINT_FACTOR;

    int32_t check_z1 = (mat_vp[2][0] * in->vertex1.x) + (mat_vp[2][1] * in->vertex1.y) + (mat_vp[2][2] * in->vertex1.z) + (mat_vp[2][3] * FIXED_POINT_FACTOR);
    int32_t check_z2 = (mat_vp[2][0] * in->vertex2.x) + (mat_vp[2][1] * in->vertex2.y) + (mat_vp[2][2] * in->vertex2.z) + (mat_vp[2][3] * FIXED_POINT_FACTOR);
    int32_t check_z3 = (mat_vp[2][0] * in->vertex3.x) + (mat_vp[2][1] * in->vertex3.y) + (mat_vp[2][2] * in->vertex3.z) + (mat_vp[2][3] * FIXED_POINT_FACTOR);

    int32_t vert_behind_camera = 0;
    if (check_z1 < z_near) vert_behind_camera++;
    if (check_z2 < z_near) vert_behind_camera++;
    if (check_z3 < z_near) vert_behind_camera++;

    if (vert_behind_camera == 0) {
        output_triangle.vertex1.x = ((mat_vp[0][0]*in->vertex1.x) + (mat_vp[0][1]*in->vertex1.y) + (mat_vp[0][2]*in->vertex1.z) + (mat_vp[0][3]*FIXED_POINT_FACTOR)) / w1;
        output_triangle.vertex1.y = ((mat_vp[1][0]*in->vertex1.x) + (mat_vp[1][1]*in->vertex1.y) + (mat_vp[1][2]*in->vertex1.z) + (mat_vp[1][3]*FIXED_POINT_FACTOR)) / w1;
        output_triangle.vertex1.z = ((mat_vp[2][0]*in->vertex1.x) + (mat_vp[2][1]*in->vertex1.y) + (mat_vp[2][2]*in->vertex1.z) + (mat_vp[2][3]*FIXED_POINT_FACTOR)) / w1;
        output_triangle.vertex2.x = ((mat_vp[0][0]*in->vertex2.x) + (mat_vp[0][1]*in->vertex2.y) + (mat_vp[0][2]*in->vertex2.z) + (mat_vp[0][3]*FIXED_POINT_FACTOR)) / w2;
        output_triangle.vertex2.y = ((mat_vp[1][0]*in->vertex2.x) + (mat_vp[1][1]*in->vertex2.y) + (mat_vp[1][2]*in->vertex2.z) + (mat_vp[1][3]*FIXED_POINT_FACTOR)) / w2;
        output_triangle.vertex2.z = ((mat_vp[2][0]*in->vertex2.x) + (mat_vp[2][1]*in->vertex2.y) + (mat_vp[2][2]*in->vertex2.z) + (mat_vp[2][3]*FIXED_POINT_FACTOR)) / w2;
        output_triangle.vertex3.x = ((mat_vp[0][0]*in->vertex3.x) + (mat_vp[0][1]*in->vertex3.y) + (mat_vp[0][2]*in->vertex3.z) + (mat_vp[0][3]*FIXED_POINT_FACTOR)) / w3;
        output_triangle.vertex3.y = ((mat_vp[1][0]*in->vertex3.x) + (mat_vp[1][1]*in->vertex3.y) + (mat_vp[1][2]*in->vertex3.z) + (mat_vp[1][3]*FIXED_POINT_FACTOR)) / w3;
        output_triangle.vertex3.z = ((mat_vp[2][0]*in->vertex3.x) + (mat_vp[2][1]*in->vertex3.y) + (mat_vp[2][2]*in->vertex3.z) + (mat_vp[2][3]*FIXED_POINT_FACTOR)) / w3;
        rearrange_parameters = 0;

    } else if (vert_behind_camera == 1) {
        if (check_z1 < z_near) {
            clip_extra_triangle(1, mat_vp, in, &output_triangle, &extra_triangle, w1, w2, w3);
            rearrange_parameters = 0;
        } else if (check_z2 < z_near) {
            clip_extra_triangle(2, mat_vp, in, &output_triangle, &extra_triangle, w2, w3, w1);
            rearrange_parameters = 1;
        } else {
            clip_extra_triangle(3, mat_vp, in, &output_triangle, &extra_triangle, w3, w1, w2);
            rearrange_parameters = 2;
        }
        render_extra_triangle = 1;

    } else if (vert_behind_camera == 2) {
        if (check_z1 > z_near) {
            clip_single_triangle(1, mat_vp, in, &output_triangle, w2, w3, w1);
            rearrange_parameters = 1;
        } else if (check_z2 > z_near) {
            clip_single_triangle(2, mat_vp, in, &output_triangle, w3, w1, w2);
            rearrange_parameters = 2;
        } else {
            clip_single_triangle(3, mat_vp, in, &output_triangle, w1, w2, w3);
            rearrange_parameters = 0;
        }
    } else {
        return;
    }

render_extra_label:;
    /* Z-range culling */
    if (output_triangle.vertex1.z > FIXED_POINT_FACTOR || output_triangle.vertex2.z > FIXED_POINT_FACTOR || output_triangle.vertex3.z > FIXED_POINT_FACTOR
        || output_triangle.vertex1.z <= 0 || output_triangle.vertex2.z <= 0 || output_triangle.vertex3.z <= 0) {
        return;
    }

    int32_t x1 = (output_triangle.vertex1.x + FIXED_POINT_FACTOR) * (SCREEN_WIDTH  - 1) / FIXED_POINT_FACTOR / 2;
    int32_t y1 = SCREEN_HEIGHT - ((output_triangle.vertex1.y + FIXED_POINT_FACTOR) * (SCREEN_HEIGHT - 1)) / FIXED_POINT_FACTOR / 2;
    int32_t x2 = (output_triangle.vertex2.x + FIXED_POINT_FACTOR) * (SCREEN_WIDTH  - 1) / FIXED_POINT_FACTOR / 2;
    int32_t y2 = SCREEN_HEIGHT - ((output_triangle.vertex2.y + FIXED_POINT_FACTOR) * (SCREEN_HEIGHT - 1)) / FIXED_POINT_FACTOR / 2;
    int32_t x3 = (output_triangle.vertex3.x + FIXED_POINT_FACTOR) * (SCREEN_WIDTH  - 1) / FIXED_POINT_FACTOR / 2;
    int32_t y3 = SCREEN_HEIGHT - ((output_triangle.vertex3.y + FIXED_POINT_FACTOR) * (SCREEN_HEIGHT - 1)) / FIXED_POINT_FACTOR / 2;

    /* Cohen-Sutherland trivial reject */
    uint8_t code1 = compute_code(x1, y1);
    uint8_t code2 = compute_code(x2, y2);
    uint8_t code3 = compute_code(x3, y3);
    if (code1 & code2 & code3) return;

    /* Backface culling */
    int32_t z1 = output_triangle.vertex1.z;
    int32_t z2 = output_triangle.vertex2.z;
    int32_t z3 = output_triangle.vertex3.z;

    int32_t v1x = x3 - x1, v1y = y3 - y1;
    int32_t v2x = x2 - x1, v2y = y2 - y1;
    int32_t Nz = v1x * v2y - v1y * v2x;
    if (Nz < 0) return;

    /* Per-vertex dynamic lighting */
    if (render_extra_triangle != -1) {
        render_lighting(in);
    }

    /* Glow shaders: reduce to base shader ID */
    if (in->shader_id > 10 && in->shader_id < 20) {
        in->shader_id -= 10;
    }

    triangle_list_next[number_triangles].vertex1.x = (int16_t)x1;
    triangle_list_next[number_triangles].vertex1.y = (int16_t)y1;
    triangle_list_next[number_triangles].vertex1.z = (int16_t)z1;
    triangle_list_next[number_triangles].vertex2.x = (int16_t)x2;
    triangle_list_next[number_triangles].vertex2.y = (int16_t)y2;
    triangle_list_next[number_triangles].vertex2.z = (int16_t)z2;
    triangle_list_next[number_triangles].vertex3.x = (int16_t)x3;
    triangle_list_next[number_triangles].vertex3.y = (int16_t)y3;
    triangle_list_next[number_triangles].vertex3.z = (int16_t)z3;

    triangle_list_next[number_triangles].shader_id  = in->shader_id;
    triangle_list_next[number_triangles].texture_id = in->texture_id;

    if (rearrange_parameters == 0) {
        triangle_list_next[number_triangles].vertex_parameter1.color = in->vertex_parameter1.color;
        triangle_list_next[number_triangles].vertex_parameter2.color = in->vertex_parameter2.color;
        triangle_list_next[number_triangles].vertex_parameter3.color = in->vertex_parameter3.color;
    } else if (rearrange_parameters == 1) {
        triangle_list_next[number_triangles].vertex_parameter1.color = in->vertex_parameter2.color;
        triangle_list_next[number_triangles].vertex_parameter2.color = in->vertex_parameter3.color;
        triangle_list_next[number_triangles].vertex_parameter3.color = in->vertex_parameter1.color;
    } else {
        triangle_list_next[number_triangles].vertex_parameter1.color = in->vertex_parameter3.color;
        triangle_list_next[number_triangles].vertex_parameter2.color = in->vertex_parameter1.color;
        triangle_list_next[number_triangles].vertex_parameter3.color = in->vertex_parameter2.color;
    }

    if (render_extra_triangle == 1 && number_triangles <= MAX_RENDER_TRIANGLES - 2) {
        render_extra_triangle = -1;
        output_triangle = extra_triangle;
        rearrange_parameters = 0;
        in->vertex_parameter1.color = triangle_list_next[number_triangles].vertex_parameter1.color;
        in->vertex_parameter2.color = triangle_list_next[number_triangles].vertex_parameter3.color;
        in->vertex_parameter3.color = triangle_list_next[number_triangles].vertex_parameter2.color;
        number_triangles++;
        goto render_extra_label;
    } else {
        number_triangles++;
    }
}
