#include "render_globals.h"
#include "chunk_globals.h"
#include "render_math.h"
#include <math.h>

float camera_position[3] = {0.0f, 0.0f, 0.0f};
int32_t camera_position_fixed_point[3] = {0, 0, 0};
float pitch = 0.0f;
float yaw = 0.0f;

#ifndef NO_GLOBAL_OFFSET
    int32_t global_offset_x;
    int32_t global_offset_z;
#endif

float mat_camera[4][4] = {{ 1.0f, 0.0f, 0.0f, 0.0f},
                          { 0.0f, 1.0f, 0.0f, 0.0f},
                          { 0.0f, 0.0f, 1.0f, 0.0f},
                          { 0.0f, 0.0f, 0.0f, 1.0f}};

float mat_cam_rotate[4][4] = {{ 1.0f, 0.0f, 0.0f, 0.0f},
                              { 0.0f, 1.0f, 0.0f, 0.0f},
                              { 0.0f, 0.0f, 1.0f, 0.0f},
                              { 0.0f, 0.0f, 0.0f, 1.0f}};

/* Perspective projection matrix.
 * CAMERA_HEIGHT is aspect-corrected: (SCREEN_HEIGHT/SCREEN_WIDTH) * CAMERA_WIDTH
 * so the 160x100 framebuffer maps to the correct FOV on a 4:3 display. */
static float mat_projection[4][4] = {
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f }
};

int32_t mat_vp[4][4] = {{ 0, 0, 0, 0},
                        { 0, 0, 0, 0},
                        { 0, 0, 0, 0},
                        { 0, 0, 0, 0}};

static int s_projection_init = 0;

static void init_projection(void) {
    mat_projection[0][0] = atanf((CAMERA_FOVX * PI / 180.0f) * 0.5f);
    /* Y component scaled by aspect ratio to prevent vertical stretching */
    mat_projection[1][1] = atanf((CAMERA_FOVY * PI / 180.0f) * 0.5f) * CAMERA_HEIGHT;
    mat_projection[2][2] = -((ZFAR + ZNEAR) / (ZFAR - ZNEAR));
    mat_projection[2][3] = -((2.0f * ZFAR * ZNEAR) / (ZFAR - ZNEAR));
    mat_projection[3][2] = -1.0f;
    s_projection_init = 1;
}

static float dot_product3(float vec1[3], float vec2[3]) {
    return vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2];
}

void update_camera(void) {
    if (!s_projection_init) init_projection();

    float cosPitch = cosf(pitch);
    float sinPitch = sinf(pitch);
    float cosYaw   = cosf(yaw);
    float sinYaw   = sinf(yaw);

    float xaxis[3] = { cosYaw, 0.0f, -sinYaw };
    float yaxis[3] = { sinYaw * sinPitch, cosPitch, cosYaw * sinPitch };
    float zaxis[3] = { sinYaw * cosPitch, -sinPitch, cosPitch * cosYaw };

    mat_camera[0][0] = xaxis[0];
    mat_camera[0][1] = xaxis[1];
    mat_camera[0][2] = xaxis[2];
    mat_camera[0][3] = -dot_product3(xaxis, camera_position);

    mat_camera[1][0] = yaxis[0];
    mat_camera[1][1] = yaxis[1];
    mat_camera[1][2] = yaxis[2];
    mat_camera[1][3] = -dot_product3(yaxis, camera_position);

    mat_camera[2][0] = zaxis[0];
    mat_camera[2][1] = zaxis[1];
    mat_camera[2][2] = zaxis[2];
    mat_camera[2][3] = -dot_product3(zaxis, camera_position);

    mat_camera[3][0] = 0.0f;
    mat_camera[3][1] = 0.0f;
    mat_camera[3][2] = 0.0f;
    mat_camera[3][3] = 1.0f;

    camera_position_fixed_point[0] = float_to_int(camera_position[0]);
    camera_position_fixed_point[1] = float_to_int(camera_position[1]);
    camera_position_fixed_point[2] = float_to_int(camera_position[2]);
}

void move_camera(float move) {
    float rotation_matrix[4][4] = {{ cosf(yaw), 0.0f,  sinf(yaw), 0.0f},
                                   { 0.0f,      1.0f,  0.0f,      0.0f},
                                   {-sinf(yaw), 0.0f,  cosf(yaw), 0.0f},
                                   { 0.0f,      0.0f,  0.0f,      1.0f}};
    float translate_matrix[4][4] = {{1.0f, 0.0f, 0.0f, 0.0f},
                                    {0.0f, 1.0f, 0.0f, 0.0f},
                                    {0.0f, 0.0f, 1.0f, move},
                                    {0.0f, 0.0f, 0.0f, 1.0f}};
    float mat_out[4][4];
    mat_mul(rotation_matrix, translate_matrix, mat_out);
    camera_position[0] -= mat_out[0][3];
    camera_position[2] -= mat_out[2][3];
}

void render_view_projection(void) {
    if (!s_projection_init) init_projection();

#ifdef NO_GLOBAL_OFFSET
    float mat_vp_float[4][4];
    mat_mul(mat_projection, mat_camera, mat_vp_float);
    mat_convert_float_fixed(mat_vp_float, mat_vp);
#else
    float mat_vp_float[4][4];
    float old_position_x = camera_position[0];
    float old_position_z = camera_position[2];

    global_offset_x = (int32_t)(camera_position[0] / CHUNK_UNITS);
    global_offset_z = (int32_t)(camera_position[2] / CHUNK_UNITS);

    camera_position[0] -= global_offset_x * CHUNK_UNITS;
    camera_position[2] -= global_offset_z * CHUNK_UNITS;

    update_camera();

    camera_position[0] = old_position_x;
    camera_position[2] = old_position_z;

    camera_position_fixed_point[0] = float_to_int(camera_position[0]);
    camera_position_fixed_point[1] = float_to_int(camera_position[1]);
    camera_position_fixed_point[2] = float_to_int(camera_position[2]);

    mat_mul(mat_projection, mat_camera, mat_vp_float);
    mat_convert_float_fixed(mat_vp_float, mat_vp);
#endif
}
