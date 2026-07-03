/* ── Image helper: wraps stb_image for Aurora FFI ── */

#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
typedef unsigned int  GLuint;
typedef unsigned int  GLenum;
typedef int           GLint;
typedef int           GLsizei;
typedef char          GLchar;
#ifndef APIENTRY
#define APIENTRY __stdcall
#endif
#else
#include <dlfcn.h>
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#endif

#ifndef AURORA_STB_IMAGE_LOADED
#define AURORA_STB_IMAGE_LOADED
#define STB_IMAGE_IMPLEMENTATION
#endif
#include "../../deps/stb_image.h"

extern "C" {

#ifdef _WIN32
static void* gl_load(const char* name) {
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    void* ptr = gl ? (void*)GetProcAddress(gl, name) : nullptr;
    if (!ptr) {
        using WGLGPA = void*(APIENTRY*)(const char*);
        static WGLGPA wgl = nullptr;
        if (!wgl) {
            HMODULE m = GetModuleHandleA("opengl32.dll");
            if (m) wgl = (WGLGPA)GetProcAddress(m, "wglGetProcAddress");
        }
        if (wgl) ptr = wgl(name);
    }
    return ptr;
}
#else
static void* gl_load(const char* name) {
    return dlsym(RTLD_DEFAULT, name);
}
#endif

typedef void (*FP_GenTextures)(GLsizei, GLuint*);
typedef void (*FP_BindTexture)(GLenum, GLuint);
typedef void (*FP_TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
typedef void (*FP_TexParameteri)(GLenum, GLenum, GLint);

static FP_GenTextures  glGenTextures_fn  = nullptr;
static FP_BindTexture  glBindTexture_fn  = nullptr;
static FP_TexImage2D   glTexImage2D_fn   = nullptr;
static FP_TexParameteri glTexParameteri_fn = nullptr;

static int gl_img_loaded = 0;
static void gl_img_ensure_loaded() {
    if (gl_img_loaded) return;
    gl_img_loaded = 1;
    glGenTextures_fn   = (FP_GenTextures)  gl_load("glGenTextures");
    glBindTexture_fn   = (FP_BindTexture)  gl_load("glBindTexture");
    glTexImage2D_fn    = (FP_TexImage2D)   gl_load("glTexImage2D");
    glTexParameteri_fn = (FP_TexParameteri)gl_load("glTexParameteri");
}

/* ── Public API ── */

void* aurora_image_load(const char* path, int* width, int* height, int* channels) {
    return stbi_load(path, width, height, channels, 0);
}

void aurora_image_free(void* data) {
    stbi_image_free(data);
}

unsigned int aurora_image_create_gl_texture(const char* path) {
    gl_img_ensure_loaded();
    if (!glGenTextures_fn || !glBindTexture_fn || !glTexImage2D_fn || !glTexParameteri_fn)
        return 0;

    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = (unsigned char*)stbi_load(path, &w, &h, &ch, 4);
    if (!pixels) return 0;

    GLuint tex = 0;
    glGenTextures_fn(1, &tex);
    if (!tex) { stbi_image_free(pixels); return 0; }

    glBindTexture_fn(0x0DE1, tex);
    glTexImage2D_fn(0x0DE1, 0, 0x1908, (GLsizei)w, (GLsizei)h, 0, 0x1908, 0x1401, pixels);
    glTexParameteri_fn(0x0DE1, 0x2801, 0x2601);
    glTexParameteri_fn(0x0DE1, 0x2800, 0x2601);
    glTexParameteri_fn(0x0DE1, 0x2802, 0x2901);
    glTexParameteri_fn(0x0DE1, 0x2803, 0x2901);

    stbi_image_free(pixels);
    return (unsigned int)tex;
}

} /* extern "C" */
