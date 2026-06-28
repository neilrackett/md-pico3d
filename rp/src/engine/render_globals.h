/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RENDER_GLOBALS_H
#define RENDER_GLOBALS_H

#include <math.h>
#include <stdint.h>

/* These are rendering globals, some needed by both cpu cores and multiple
 * functions in the rendering subsystem */

typedef uint16_t color_t;

#define SCREEN_WIDTH  160   /* internal render width (pixel-doubled to 320) */
#define SCREEN_HEIGHT 100   /* internal render height (pixel-doubled to 200) */
#define DISPLAY_WIDTH  320  /* Atari ST output resolution */
#define DISPLAY_HEIGHT 200
#define PI 3.141592654f

/* Graphics adjustments */
#define MAX_RENDER_TRIANGLES 1000
extern uint32_t number_triangles;

extern uint8_t skip_frame;
extern uint8_t shader_override;
#ifdef DEBUG_INFO
extern uint32_t rendered_triangles;
#endif

/* full 32 bit fixed point vertex */
struct vertex_32 {
    int32_t x;
    int32_t y;
    int32_t z;
};

/* reduced range 16 bit vertex point */
struct vertex_16 {
    int16_t x;
    int16_t y;
    int16_t z;
};

struct vertex_floating_point {
    float x;
    float y;
    float z;
};

/* UV maps share storage with vertex colors since only one or the other is used */
union color_or_uv {
    color_t color;
    uint8_t uv[2];
};

/* default triangle for world coordinates and final transform to view space */
struct triangle_32 {
    struct vertex_32 vertex1;
    struct vertex_32 vertex2;
    struct vertex_32 vertex3;
    uint8_t shader_id;
    uint8_t texture_id;
    union color_or_uv vertex_parameter1;
    union color_or_uv vertex_parameter2;
    union color_or_uv vertex_parameter3;
};

/* reduced range triangle using 16 bits fixed point - 28 bytes each */
struct triangle_16 {
    struct vertex_16 vertex1;
    struct vertex_16 vertex2;
    struct vertex_16 vertex3;
    uint8_t shader_id;
    uint8_t texture_id;
    union color_or_uv vertex_parameter1;
    union color_or_uv vertex_parameter2;
    union color_or_uv vertex_parameter3;
    uint8_t chunk_x;
    uint8_t chunk_y;
};

struct triangle_floating_point {
    struct vertex_floating_point vertex1;
    struct vertex_floating_point vertex2;
    struct vertex_floating_point vertex3;
    uint8_t shader_id;
    uint8_t texture_id;
    union color_or_uv vertex_parameter1;
    union color_or_uv vertex_parameter2;
    union color_or_uv vertex_parameter3;
};

extern struct triangle_16 *triangle_list_current;
extern struct triangle_16 *triangle_list_next;

/* textures stored in a list so the rasterizer can access them by ID */
struct texture {
    uint8_t texture_size;
    uint8_t unused1;
    uint16_t unused2;
    const color_t *image;
};

/* lighting system */
struct light {
    struct vertex_32 position;
};

struct chunk_lighting {
    int32_t number;
    const struct light *lights;
};

extern int8_t light_falloff;
#define MAX_FALLOFF 4
#define LIGHT_DISTANCE (FIXED_POINT_FACTOR * 50000)
extern color_t sky;

/* 3d transformation matrices */
extern float camera_position[3];
extern int32_t camera_position_fixed_point[3];
extern float pitch;
extern float yaw;

#ifndef NO_GLOBAL_OFFSET
    extern int32_t global_offset_x;
    extern int32_t global_offset_z;
#endif

extern float mat_camera[4][4];
extern float mat_cam_rotate[4][4];

/* perspective matrix */
#define CAMERA_WIDTH 1.0f
/* Aspect-corrected height: SCREEN_HEIGHT/SCREEN_WIDTH = 100/160 = 0.625 */
#define CAMERA_HEIGHT (CAMERA_WIDTH * (float)SCREEN_HEIGHT / (float)SCREEN_WIDTH)
#define ZFAR 40
#define ZNEAR 0.25f
#define CAMERA_FOVX 180.0f
#define CAMERA_FOVY 180.0f

extern int32_t mat_vp[4][4];

extern uint8_t animated_texture_offset;
extern uint8_t animated_texture_counter;

void update_camera(void);
void move_camera(float move);
void render_view_projection(void);

void clip_single_triangle(uint8_t vertex_in_screen, int32_t vp[4][4],
    struct triangle_32 *input_triangle, struct triangle_32 *output_triangle,
    int32_t w1, int32_t w2, int32_t w3);
void clip_extra_triangle(uint8_t vertex_out_screen, int32_t vp[4][4],
    struct triangle_32 *input_triangle, struct triangle_32 *output_triangle,
    struct triangle_32 *extra, int32_t w1, int32_t w2, int32_t w3);

uint32_t render_view_frustum_culling(int32_t x, int32_t y, int32_t z,
    int32_t x_offset, int32_t y_offset, int32_t z_offset);

void render_lighting(struct triangle_32 *in);

void render_model_16bit(struct triangle_16 *model, int32_t triangle_count);
void render_model_32bit(struct triangle_32 *model, int32_t triangle_count);
void render_model_16bit_flash(const struct triangle_16 *model, int32_t triangle_count);
void render_model_32bit_flash(const struct triangle_32 *model, int32_t triangle_count);

void render_rasterize(uint32_t num_triangle, color_t *fb);

int32_t render_sync(void);

void render_triangle(struct triangle_32 *in);

#endif /* RENDER_GLOBALS_H */
