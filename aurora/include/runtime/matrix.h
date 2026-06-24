#pragma once
#include <cstdint>

extern "C" {

void* aurora_mat4_new();
void  aurora_mat4_free(void* m);
void  aurora_mat4_identity(void* m);
void  aurora_mat4_mul(void* out, const void* a, const void* b);
void  aurora_mat4_translate(void* out, float x, float y, float z);
void  aurora_mat4_rotate(void* out, float angle_deg, float x, float y, float z);
void  aurora_mat4_scale(void* out, float x, float y, float z);
void  aurora_mat4_perspective(void* out, float fov_deg, float aspect, float near, float far);
void  aurora_mat4_lookat(void* out, float eye_x, float eye_y, float eye_z,
                         float target_x, float target_y, float target_z,
                         float up_x, float up_y, float up_z);
void  aurora_mat4_copy(void* dst, const void* src);

/* Pre-built cube vertex data for modern GL demos (interleaved pos3+color3) */
const void* aurora_gl_cube_vertices();
int         aurora_gl_cube_vertex_count();
int         aurora_gl_cube_vertex_stride();

}
