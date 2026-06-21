#include "../../../include/runtime/matrix.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

extern "C" {

void* aurora_mat4_new() {
    float* m = (float*)calloc(16, sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    return m;
}

void aurora_mat4_free(void* m) {
    free(m);
}

void aurora_mat4_identity(void* m) {
    if (!m) return;
    float* M = (float*)m;
    memset(M, 0, 16 * sizeof(float));
    M[0] = M[5] = M[10] = M[15] = 1.0f;
}

void aurora_mat4_copy(void* dst, const void* src) {
    if (!dst || !src) return;
    memcpy(dst, src, 16 * sizeof(float));
}

void aurora_mat4_mul(void* out, const void* a, const void* b) {
    if (!out || !a || !b) return;
    const float* A = (const float*)a;
    const float* B = (const float*)b;
    float* O = (float*)out;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++)
                sum += A[row + k * 4] * B[k + col * 4];
            O[row + col * 4] = sum;
        }
    }
}

void aurora_mat4_translate(void* out, float x, float y, float z) {
    float* M = (float*)out;
    aurora_mat4_identity(M);
    M[12] = x;
    M[13] = y;
    M[14] = z;
}

void aurora_mat4_scale(void* out, float x, float y, float z) {
    float* M = (float*)out;
    memset(M, 0, 16 * sizeof(float));
    M[0]  = x;
    M[5]  = y;
    M[10] = z;
    M[15] = 1.0f;
}

void aurora_mat4_rotate(void* out, float angle_deg, float ax, float ay, float az) {
    float rad = angle_deg * 3.14159265f / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    float t = 1.0f - c;
    float len = sqrtf(ax * ax + ay * ay + az * az);
    if (len < 1e-8f) { aurora_mat4_identity(out); return; }
    float x = ax / len, y = ay / len, z = az / len;
    float* M = (float*)out;
    M[0]  = t*x*x + c;    M[4]  = t*x*y - s*z;  M[8]  = t*x*z + s*y;  M[12] = 0.0f;
    M[1]  = t*x*y + s*z;  M[5]  = t*y*y + c;    M[9]  = t*y*z - s*x;  M[13] = 0.0f;
    M[2]  = t*x*z - s*y;  M[6]  = t*y*z + s*x;  M[10] = t*z*z + c;    M[14] = 0.0f;
    M[3]  = 0.0f;          M[7]  = 0.0f;          M[11] = 0.0f;          M[15] = 1.0f;
}

void aurora_mat4_perspective(void* out, float fov_deg, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov_deg * 3.14159265f / 360.0f);
    float* M = (float*)out;
    memset(M, 0, 16 * sizeof(float));
    M[0]  = f / aspect;
    M[5]  = f;
    M[10] = (far + near) / (near - far);
    M[11] = -1.0f;
    M[14] = 2.0f * far * near / (near - far);
}

void aurora_mat4_lookat(void* out, float eye_x, float eye_y, float eye_z,
                        float target_x, float target_y, float target_z,
                        float up_x, float up_y, float up_z) {
    float fwd_x = target_x - eye_x, fwd_y = target_y - eye_y, fwd_z = target_z - eye_z;
    float fwd_len = sqrtf(fwd_x*fwd_x + fwd_y*fwd_y + fwd_z*fwd_z);
    if (fwd_len < 1e-8f) { aurora_mat4_identity(out); return; }
    fwd_x /= fwd_len; fwd_y /= fwd_len; fwd_z /= fwd_len;
    float side_x = fwd_y*up_z - fwd_z*up_y;
    float side_y = fwd_z*up_x - fwd_x*up_z;
    float side_z = fwd_x*up_y - fwd_y*up_x;
    float side_len = sqrtf(side_x*side_x + side_y*side_y + side_z*side_z);
    if (side_len < 1e-8f) { aurora_mat4_identity(out); return; }
    side_x /= side_len; side_y /= side_len; side_z /= side_len;
    float uup_x = side_y*fwd_z - side_z*fwd_y;
    float uup_y = side_z*fwd_x - side_x*fwd_z;
    float uup_z = side_x*fwd_y - side_y*fwd_x;
    float* M = (float*)out;
    M[0] = side_x; M[4] = side_y; M[8]  = side_z;  M[12] = -(side_x*eye_x + side_y*eye_y + side_z*eye_z);
    M[1] = uup_x;  M[5] = uup_y;  M[9]  = uup_z;   M[13] = -(uup_x*eye_x + uup_y*eye_y + uup_z*eye_z);
    M[2] = -fwd_x; M[6] = -fwd_y; M[10] = -fwd_z;  M[14] =  (fwd_x*eye_x + fwd_y*eye_y + fwd_z*eye_z);
    M[3] = 0.0f;   M[7] = 0.0f;   M[11] = 0.0f;    M[15] = 1.0f;
}

/* ── Pre-built cube vertex data (interleaved pos3+color3, 36 triangles) ── */
static const float g_cube_vertices[] = {
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
};

const void* aurora_gl_cube_vertices() { return g_cube_vertices; }
int aurora_gl_cube_vertex_count() { return 36; }
int aurora_gl_cube_vertex_stride() { return 6 * (int)sizeof(float); }

} /* extern "C" */
