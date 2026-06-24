/* ── GPU-accelerated 2D sprite batcher ──
 * Batches textured quads into a single draw call.
 * Designed for 2D games (Flappy Bird, platformers, etc.)
 *
 * All functions are extern "C" for Aurora FFI.
 * Each function loads its own GL function pointers dynamically.
 */

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "../../deps/stb_image.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstddef>
typedef unsigned int  GLuint;
typedef unsigned int  GLenum;
typedef int           GLint;
typedef int           GLsizei;
typedef float         GLfloat;
typedef char          GLchar;
typedef unsigned char GLboolean;
typedef std::ptrdiff_t GLsizeiptr;
typedef std::ptrdiff_t GLintptr;
typedef unsigned int   GLbitfield;
#else
#include <dlfcn.h>
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#endif

/* ── Constants ── */
#define SPRITE2D_MAX_SPRITES  4096
#define SPRITE2D_MAX_TEXTURES 256
#define SPRITE2D_VERTEX_SIZE   8  /* pos(2) + uv(2) + color(4) = 8 floats */

extern "C" {

/* ── GL function pointer loading ── */
/* Matches gl_helper.cpp's known-working gl_load() exactly. */
#ifdef _WIN32
static void* gl_sprite_load(const char* name) {
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    void* ptr = gl ? (void*)GetProcAddress(gl, name) : nullptr;
    if (!ptr) {
        using WGLGPA = void*(__stdcall*)(const char*);
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
static void* gl_sprite_load(const char* name) {
    return dlsym(RTLD_DEFAULT, name);
}
#endif

/* ── Function pointer types ── */
typedef void          (*FP_GenBuffers)(GLsizei, GLuint*);
typedef void          (*FP_BindBuffer)(GLenum, GLuint);
typedef void          (*FP_BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void          (*FP_BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*);
typedef void          (*FP_GenVertexArrays)(GLsizei, GLuint*);
typedef void          (*FP_BindVertexArray)(GLuint);
typedef void          (*FP_VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void          (*FP_EnableVertexAttribArray)(GLuint);
typedef void          (*FP_DisableVertexAttribArray)(GLuint);
typedef void          (*FP_DrawElements)(GLenum, GLsizei, GLenum, const void*);
typedef void          (*FP_DrawArrays)(GLenum, GLint, GLsizei);
typedef GLuint        (*FP_CreateShader)(GLenum);
typedef GLuint        (*FP_CreateProgram)(void);
typedef void          (*FP_ShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*);
typedef void          (*FP_CompileShader)(GLuint);
typedef void          (*FP_GetShaderiv)(GLuint, GLenum, GLint*);
typedef void          (*FP_GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void          (*FP_GetProgramiv)(GLuint, GLenum, GLint*);
typedef void          (*FP_GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void          (*FP_AttachShader)(GLuint, GLuint);
typedef void          (*FP_LinkProgram)(GLuint);
typedef void          (*FP_UseProgram)(GLuint);
typedef void          (*FP_DeleteShader)(GLuint);
typedef void          (*FP_DeleteProgram)(GLuint);
typedef GLint         (*FP_GetUniformLocation)(GLuint, const GLchar*);
typedef void          (*FP_Uniform1i)(GLint, GLint);
typedef void          (*FP_UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void          (*FP_GenTextures)(GLsizei, GLuint*);
typedef void          (*FP_BindTexture)(GLenum, GLuint);
typedef void          (*FP_TexParameteri)(GLenum, GLenum, GLint);
typedef void          (*FP_TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
typedef void          (*FP_Enable)(GLenum);
typedef void          (*FP_Disable)(GLenum);
typedef void          (*FP_BlendFunc)(GLenum, GLenum);
typedef void          (*FP_ActiveTexture)(GLenum);
typedef void          (*FP_Viewport)(GLint, GLint, GLsizei, GLsizei);
typedef void          (*FP_Clear)(GLbitfield);
typedef void          (*FP_ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void          (*FP_DeleteTextures)(GLsizei, const GLuint*);
typedef void*         (*FP_MapBuffer)(GLenum, GLenum);
typedef GLboolean     (*FP_UnmapBuffer)(GLenum);
typedef void          (*FP_Uniform1f)(GLint, GLfloat);
typedef void          (*FP_DeleteVertexArrays)(GLsizei, const GLuint*);
typedef void          (*FP_DeleteBuffers)(GLsizei, const GLuint*);

/* ── Function pointers ── */
static FP_GenBuffers           glGenBuffers_fn           = nullptr;
static FP_BindBuffer           glBindBuffer_fn           = nullptr;
static FP_BufferData           glBufferData_fn           = nullptr;
static FP_BufferSubData        glBufferSubData_fn        = nullptr;
static FP_GenVertexArrays      glGenVertexArrays_fn      = nullptr;
static FP_BindVertexArray      glBindVertexArray_fn      = nullptr;
static FP_VertexAttribPointer  glVertexAttribPointer_fn  = nullptr;
static FP_EnableVertexAttribArray glEnableVertexAttribArray_fn = nullptr;
static FP_DisableVertexAttribArray glDisableVertexAttribArray_fn = nullptr;
static FP_DrawElements         glDrawElements_fn         = nullptr;
static FP_DrawArrays           glDrawArrays_fn           = nullptr;
static FP_CreateShader         glCreateShader_fn         = nullptr;
static FP_CreateProgram        glCreateProgram_fn        = nullptr;
static FP_ShaderSource         glShaderSource_fn         = nullptr;
static FP_CompileShader        glCompileShader_fn        = nullptr;
static FP_GetShaderiv          glGetShaderiv_fn          = nullptr;
static FP_GetShaderInfoLog     glGetShaderInfoLog_fn     = nullptr;
static FP_GetProgramiv         glGetProgramiv_fn         = nullptr;
static FP_GetProgramInfoLog    glGetProgramInfoLog_fn    = nullptr;
static FP_AttachShader         glAttachShader_fn         = nullptr;
static FP_LinkProgram          glLinkProgram_fn          = nullptr;
static FP_UseProgram           glUseProgram_fn           = nullptr;
static FP_DeleteShader         glDeleteShader_fn         = nullptr;
static FP_DeleteProgram        glDeleteProgram_fn        = nullptr;
static FP_GetUniformLocation   glGetUniformLocation_fn   = nullptr;
static FP_Uniform1i            glUniform1i_fn            = nullptr;
static FP_UniformMatrix4fv     glUniformMatrix4fv_fn     = nullptr;
static FP_GenTextures          glGenTextures_fn          = nullptr;
static FP_BindTexture          glBindTexture_fn          = nullptr;
static FP_TexParameteri        glTexParameteri_fn        = nullptr;
static FP_TexImage2D           glTexImage2D_fn           = nullptr;
static FP_Enable               glEnable_fn               = nullptr;
static FP_Disable              glDisable_fn              = nullptr;
static FP_BlendFunc            glBlendFunc_fn            = nullptr;
static FP_ActiveTexture        glActiveTexture_fn        = nullptr;
static FP_Viewport             glViewport_fn             = nullptr;
static FP_Clear                glClear_fn                = nullptr;
static FP_ClearColor           glClearColor_fn           = nullptr;
static FP_DeleteTextures       glDeleteTextures_fn       = nullptr;
static FP_MapBuffer            glMapBuffer_fn            = nullptr;
static FP_UnmapBuffer          glUnmapBuffer_fn          = nullptr;
static FP_Uniform1f            glUniform1f_fn            = nullptr;
static FP_DeleteVertexArrays   glDeleteVertexArrays_fn   = nullptr;
static FP_DeleteBuffers        glDeleteBuffers_fn        = nullptr;

static int s_gl_loaded = 0;
static void ensure_gl_loaded() {
    if (s_gl_loaded) return;
    s_gl_loaded = 1;
    glGenBuffers_fn           = (FP_GenBuffers)          gl_sprite_load("glGenBuffers");
    glBindBuffer_fn           = (FP_BindBuffer)          gl_sprite_load("glBindBuffer");
    glBufferData_fn           = (FP_BufferData)          gl_sprite_load("glBufferData");
    glBufferSubData_fn        = (FP_BufferSubData)       gl_sprite_load("glBufferSubData");
    glGenVertexArrays_fn      = (FP_GenVertexArrays)     gl_sprite_load("glGenVertexArrays");
    glBindVertexArray_fn      = (FP_BindVertexArray)     gl_sprite_load("glBindVertexArray");
    glVertexAttribPointer_fn  = (FP_VertexAttribPointer) gl_sprite_load("glVertexAttribPointer");
    glEnableVertexAttribArray_fn = (FP_EnableVertexAttribArray) gl_sprite_load("glEnableVertexAttribArray");
    glDisableVertexAttribArray_fn = (FP_DisableVertexAttribArray) gl_sprite_load("glDisableVertexAttribArray");
    glDrawElements_fn         = (FP_DrawElements)        gl_sprite_load("glDrawElements");
    glDrawArrays_fn           = (FP_DrawArrays)          gl_sprite_load("glDrawArrays");
    glCreateShader_fn         = (FP_CreateShader)        gl_sprite_load("glCreateShader");
    glCreateProgram_fn        = (FP_CreateProgram)       gl_sprite_load("glCreateProgram");
    glShaderSource_fn         = (FP_ShaderSource)        gl_sprite_load("glShaderSource");
    glCompileShader_fn        = (FP_CompileShader)       gl_sprite_load("glCompileShader");
    glGetShaderiv_fn           = (FP_GetShaderiv)          gl_sprite_load("glGetShaderiv");
    glGetShaderInfoLog_fn     = (FP_GetShaderInfoLog)     gl_sprite_load("glGetShaderInfoLog");
    glGetProgramiv_fn         = (FP_GetProgramiv)         gl_sprite_load("glGetProgramiv");
    glGetProgramInfoLog_fn    = (FP_GetProgramInfoLog)    gl_sprite_load("glGetProgramInfoLog");
    glAttachShader_fn        = (FP_AttachShader)         gl_sprite_load("glAttachShader");
    glLinkProgram_fn          = (FP_LinkProgram)         gl_sprite_load("glLinkProgram");
    glUseProgram_fn           = (FP_UseProgram)          gl_sprite_load("glUseProgram");
    glDeleteShader_fn         = (FP_DeleteShader)        gl_sprite_load("glDeleteShader");
    glDeleteProgram_fn        = (FP_DeleteProgram)       gl_sprite_load("glDeleteProgram");
    glGetUniformLocation_fn   = (FP_GetUniformLocation)  gl_sprite_load("glGetUniformLocation");
    glUniform1i_fn            = (FP_Uniform1i)           gl_sprite_load("glUniform1i");
    glUniformMatrix4fv_fn     = (FP_UniformMatrix4fv)    gl_sprite_load("glUniformMatrix4fv");
    glGenTextures_fn          = (FP_GenTextures)         gl_sprite_load("glGenTextures");
    glBindTexture_fn          = (FP_BindTexture)         gl_sprite_load("glBindTexture");
    glTexParameteri_fn        = (FP_TexParameteri)       gl_sprite_load("glTexParameteri");
    glTexImage2D_fn           = (FP_TexImage2D)          gl_sprite_load("glTexImage2D");
    glEnable_fn               = (FP_Enable)              gl_sprite_load("glEnable");
    glDisable_fn              = (FP_Disable)             gl_sprite_load("glDisable");
    glBlendFunc_fn            = (FP_BlendFunc)           gl_sprite_load("glBlendFunc");
    glActiveTexture_fn        = (FP_ActiveTexture)       gl_sprite_load("glActiveTexture");
    glViewport_fn             = (FP_Viewport)            gl_sprite_load("glViewport");
    glClear_fn                = (FP_Clear)               gl_sprite_load("glClear");
    glClearColor_fn           = (FP_ClearColor)          gl_sprite_load("glClearColor");
    glDeleteTextures_fn       = (FP_DeleteTextures)      gl_sprite_load("glDeleteTextures");
    glMapBuffer_fn            = (FP_MapBuffer)           gl_sprite_load("glMapBuffer");
    glUnmapBuffer_fn          = (FP_UnmapBuffer)         gl_sprite_load("glUnmapBuffer");
    glUniform1f_fn            = (FP_Uniform1f)           gl_sprite_load("glUniform1f");
    glDeleteVertexArrays_fn   = (FP_DeleteVertexArrays)  gl_sprite_load("glDeleteVertexArrays");
    glDeleteBuffers_fn        = (FP_DeleteBuffers)       gl_sprite_load("glDeleteBuffers");

}

/* ── Constants (avoid GL header dependency) ── */
#define GL_ARRAY_BUFFER           0x8892
#define GL_ELEMENT_ARRAY_BUFFER   0x8893
#define GL_STATIC_DRAW            0x88E4
#define GL_DYNAMIC_DRAW           0x88E8
#define GL_STREAM_DRAW            0x88E0
#define GL_FALSE                  0
#define GL_TRUE                   1
#define GL_FLOAT                  0x1406
#define GL_UNSIGNED_SHORT         0x1403
#define GL_UNSIGNED_BYTE          0x1401
#define GL_TRIANGLES              0x0004
#define GL_TRIANGLE_STRIP         0x0005
#define GL_TRIANGLE_FAN           0x0006
#define GL_LINES                  0x0001
#define GL_TEXTURE_2D             0x0DE1
#define GL_TEXTURE0               0x84C0
#define GL_TEXTURE_WRAP_S         0x2802
#define GL_TEXTURE_WRAP_T         0x2803
#define GL_TEXTURE_MIN_FILTER     0x2801
#define GL_TEXTURE_MAG_FILTER     0x2800
#define GL_NEAREST                0x2600
#define GL_LINEAR                 0x2601
#define GL_REPEAT                 0x2901
#define GL_CLAMP_TO_EDGE          0x812F
#define GL_RGB                    0x1907
#define GL_RGBA                   0x1908
#define GL_UNSIGNED_BYTE_EXT      0x1401
#define GL_UNSIGNED_INT          0x1405
#define GL_BLEND                  0x0BE2
#define GL_SRC_ALPHA              0x0302
#define GL_ONE_MINUS_SRC_ALPHA    0x0303
#define GL_COLOR_BUFFER_BIT       0x00004000
#define GL_DEPTH_BUFFER_BIT       0x00000100
#define GL_WRITE_ONLY             0x88B9
#define GL_VERTEX_SHADER          0x8B31
#define GL_FRAGMENT_SHADER        0x8B30
#define GL_COMPILE_STATUS         0x8B81
#define GL_LINK_STATUS            0x8B82
#define GL_INFO_LOG_LENGTH        0x8B84

/* ── Sprite vertex ── */
struct SpriteVertex {
    float x, y;    /* position */
    float u, v;    /* texcoord */
    float r, g, b, a; /* color */
};

/* ── Global state ── */
static GLuint    g_vao = 0;
static GLuint    g_vbo = 0;
static GLuint    g_ibo = 0;
static GLuint    g_program = 0;
static GLint     g_tex_uniform = -1;
static GLint     g_proj_uniform = -1;

static int       g_max_sprites = 0;
static int       g_sprite_count = 0;
static int       g_screen_w = 800;
static int       g_screen_h = 600;

static SpriteVertex* g_vertex_data = nullptr;
static GLuint*       g_texture_ids = nullptr; /* per-sprite texture id for sorting */

/* Texture registry */
static struct {
    GLuint gl_id;
    char   name[260];
    int    w, h;
} g_textures[SPRITE2D_MAX_TEXTURES];
static int g_texture_count = 0;

static int g_initialized = 0;

/* ── Built-in shaders ── */
static const char* g_vert_src =
    "#version 330 core\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aUV;\n"
    "layout(location=2) in vec4 aColor;\n"
    "uniform mat4 uProj;\n"
    "out vec2 vUV;\n"
    "out vec4 vColor;\n"
    "void main() {\n"
    "    gl_Position = uProj * vec4(aPos, 0.0, 1.0);\n"
    "    vUV = aUV;\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* g_frag_src =
    "#version 330 core\n"
    "in vec2 vUV;\n"
    "in vec4 vColor;\n"
    "uniform sampler2D uTex;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = texture(uTex, vUV) * vColor;\n"
    "}\n";

static GLuint create_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader_fn(type);
    if (!shader) return 0;
    glShaderSource_fn(shader, 1, &src, nullptr);
    glCompileShader_fn(shader);
    GLint ok = 0;
    glGetShaderiv_fn(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {0};
        GLsizei len = 0;
        glGetShaderInfoLog_fn(shader, 1024, &len, log);
        fprintf(stderr, "[sprite2d] shader compile error (type=%u): %s\n", (unsigned)type, log);
        glDeleteShader_fn(shader);
        return 0;
    }
    return shader;
}

static GLuint create_program(const char* vs, const char* fs) {
    GLuint vs_id = create_shader(GL_VERTEX_SHADER, vs);
    GLuint fs_id = create_shader(GL_FRAGMENT_SHADER, fs);
    if (!vs_id || !fs_id) {
        if (vs_id) glDeleteShader_fn(vs_id);
        if (fs_id) glDeleteShader_fn(fs_id);
        return 0;
    }
    GLuint prog = glCreateProgram_fn();
    glAttachShader_fn(prog, vs_id);
    glAttachShader_fn(prog, fs_id);
    glLinkProgram_fn(prog);
    GLint ok = 0;
    glGetProgramiv_fn(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = {0};
        GLsizei len = 0;
        glGetProgramInfoLog_fn(prog, 1024, &len, log);
        fprintf(stderr, "[sprite2d] program link error: %s\n", log);
        glDeleteShader_fn(vs_id);
        glDeleteShader_fn(fs_id);
        glDeleteProgram_fn(prog);
        return 0;
    }
    glDeleteShader_fn(vs_id);
    glDeleteShader_fn(fs_id);
    return prog;
}

/* ── Public API ── */

void aurora_sprite2d_init(int max_sprites) {
    if (g_initialized) return;
    ensure_gl_loaded();

    if (max_sprites <= 0) max_sprites = SPRITE2D_MAX_SPRITES;
    if (max_sprites > SPRITE2D_MAX_SPRITES) max_sprites = SPRITE2D_MAX_SPRITES;
    g_max_sprites = max_sprites;
    g_sprite_count = 0;

    /* Allocate vertex + texture arrays */
    g_vertex_data = (SpriteVertex*)calloc((size_t)g_max_sprites * 4, sizeof(SpriteVertex));
    g_texture_ids = (GLuint*)calloc((size_t)g_max_sprites, sizeof(GLuint));
    if (!g_vertex_data || !g_texture_ids) {
        fprintf(stderr, "[sprite2d] allocation failed\n");
        free(g_vertex_data);
        free(g_texture_ids);
        g_vertex_data = nullptr;
        g_texture_ids = nullptr;
        return;
    }

    /* Build index buffer (static: 6 indices per quad) */
    GLuint* indices = (GLuint*)malloc((size_t)g_max_sprites * 6 * sizeof(GLuint));
    if (!indices) {
        fprintf(stderr, "[sprite2d] index allocation failed\n");
        free(g_vertex_data);
        free(g_texture_ids);
        g_vertex_data = nullptr;
        g_texture_ids = nullptr;
        return;
    }
    for (int i = 0; i < g_max_sprites; i++) {
        indices[i * 6 + 0] = (GLuint)(i * 4 + 0);
        indices[i * 6 + 1] = (GLuint)(i * 4 + 1);
        indices[i * 6 + 2] = (GLuint)(i * 4 + 2);
        indices[i * 6 + 3] = (GLuint)(i * 4 + 2);
        indices[i * 6 + 4] = (GLuint)(i * 4 + 3);
        indices[i * 6 + 5] = (GLuint)(i * 4 + 0);
    }

    /* Create GL buffers */
    glGenBuffers_fn(1, &g_vbo);
    glGenBuffers_fn(1, &g_ibo);
    glGenVertexArrays_fn(1, &g_vao);

    glBindVertexArray_fn(g_vao);

    /* VBO (dynamic, will be updated every frame) */
    glBindBuffer_fn(GL_ARRAY_BUFFER, g_vbo);
    glBufferData_fn(GL_ARRAY_BUFFER, (GLsizeiptr)g_max_sprites * 4 * sizeof(SpriteVertex),
                    nullptr, GL_DYNAMIC_DRAW);

    /* IBO (static) */
    glBindBuffer_fn(GL_ELEMENT_ARRAY_BUFFER, g_ibo);
    glBufferData_fn(GL_ELEMENT_ARRAY_BUFFER,
                    (GLsizeiptr)g_max_sprites * 6 * sizeof(GLuint),
                    indices, GL_STATIC_DRAW);
    free(indices);

    /* Vertex attribs: pos(2), uv(2), color(4) */
    glVertexAttribPointer_fn(0, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void*)0);
    glEnableVertexAttribArray_fn(0);
    glVertexAttribPointer_fn(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                              (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray_fn(1);
    glVertexAttribPointer_fn(2, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                              (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray_fn(2);

    glBindVertexArray_fn(0);

    /* Create shader program */
    g_program = create_program(g_vert_src, g_frag_src);
    if (!g_program) {
        fprintf(stderr, "[sprite2d] failed to create shader program\n");
        return;
    }
    g_tex_uniform = glGetUniformLocation_fn(g_program, "uTex");
    g_proj_uniform = glGetUniformLocation_fn(g_program, "uProj");

    /* Enable blending */
    glEnable_fn(GL_BLEND);
    glBlendFunc_fn(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Create a default white texture (tex_id 0) */
    {
        unsigned char white_pixel[4] = { 255, 255, 255, 255 };
        GLuint tex = 0;
        glGenTextures_fn(1, &tex);
        glBindTexture_fn(GL_TEXTURE_2D, tex);
        glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D_fn(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                        GL_UNSIGNED_BYTE, white_pixel);
        g_textures[0].gl_id = tex;
        g_textures[0].name[0] = 0;
        g_textures[0].w = 1;
        g_textures[0].h = 1;
        g_texture_count = 1;
    }

    g_initialized = 1;
}

void aurora_sprite2d_set_viewport(int w, int h) {
    if (w <= 0) w = 800;
    if (h <= 0) h = 600;
    g_screen_w = w;
    g_screen_h = h;
    if (glViewport_fn) glViewport_fn(0, 0, w, h);

    /* Update projection matrix (orthographic, pixel-perfect) */
    if (g_program && g_proj_uniform >= 0) {
        glUseProgram_fn(g_program);
        /* Simple ortho: left=0, right=w, bottom=h, top=0 */
        float l = 0.0f, r = (float)w, b = (float)h, t = 0.0f;
        float n = -1.0f, f2 = 1.0f;
        float proj[16] = {0};
        proj[0]  = 2.0f / (r - l);
        proj[5]  = 2.0f / (t - b);
        proj[10] = -2.0f / (f2 - n);
        proj[12] = -(r + l) / (r - l);
        proj[13] = -(t + b) / (t - b);
        proj[14] = -(f2 + n) / (f2 - n);
        proj[15] = 1.0f;
        glUniformMatrix4fv_fn(g_proj_uniform, 1, GL_FALSE, proj);
    }
}

int aurora_sprite2d_load_texture(const char* path) {
    if (!path || !path[0]) return 0;
    ensure_gl_loaded();

    /* Check if already loaded */
    for (int i = 0; i < g_texture_count; i++) {
        if (strcmp(g_textures[i].name, path) == 0) return i;
    }

    if (g_texture_count >= SPRITE2D_MAX_TEXTURES) return 0;

    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load(path, &w, &h, &ch, 4);
    if (!pixels) {
        fprintf(stderr, "[sprite2d] failed to load texture: %s\n", path);
        return 0;
    }

    int id = g_texture_count;
    GLuint tex = 0;
    glGenTextures_fn(1, &tex);
    glBindTexture_fn(GL_TEXTURE_2D, tex);
    glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D_fn(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    g_textures[id].gl_id = tex;
    strncpy(g_textures[id].name, path, 259);
    g_textures[id].name[259] = 0;
    g_textures[id].w = w;
    g_textures[id].h = h;
    g_texture_count++;

    return id;
}

int aurora_sprite2d_create_texture(int w, int h, const unsigned char* data) {
    if (g_texture_count >= SPRITE2D_MAX_TEXTURES) return 0;
    ensure_gl_loaded();

    int id = g_texture_count;
    GLuint tex = 0;
    glGenTextures_fn(1, &tex);
    glBindTexture_fn(GL_TEXTURE_2D, tex);
    glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri_fn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D_fn(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, data);

    g_textures[id].gl_id = tex;
    g_textures[id].name[0] = 0;
    g_textures[id].w = w;
    g_textures[id].h = h;
    g_texture_count++;

    return id;
}

void aurora_sprite2d_draw(int tex_id, float x, float y, float w, float h,
                           float rot, float r, float g, float b, float a,
                           float u, float v, float uw, float vh) {
    if (!g_initialized || g_sprite_count >= g_max_sprites) return;

    if (tex_id < 0 || tex_id >= g_texture_count) tex_id = 0;

    int idx = g_sprite_count;
    g_texture_ids[idx] = g_textures[tex_id].gl_id;

    float hw = w * 0.5f;
    float hh = h * 0.5f;

    SpriteVertex* verts = &g_vertex_data[idx * 4];

    if (rot == 0.0f) {
        /* Axis-aligned: fast path */
        float x0 = x - hw, y0 = y - hh;
        float x1 = x + hw, y1 = y + hh;
        verts[0] = {x0, y0, u, v, r, g, b, a};
        verts[1] = {x1, y0, u + uw, v, r, g, b, a};
        verts[2] = {x1, y1, u + uw, v + vh, r, g, b, a};
        verts[3] = {x0, y1, u, v + vh, r, g, b, a};
    } else {
        /* Rotated: compute corners */
        float c = (float)cos(rot), s = (float)sin(rot);
        float cx = x, cy = y;
        float px[4] = {-hw, hw, hw, -hw};
        float py[4] = {-hh, -hh, hh, hh};
        for (int i = 0; i < 4; i++) {
            float rx = px[i] * c - py[i] * s;
            float ry = px[i] * s + py[i] * c;
            verts[i].x = cx + rx;
            verts[i].y = cy + ry;
        }
        verts[0].u = u;       verts[0].v = v;
        verts[1].u = u + uw;  verts[1].v = v;
        verts[2].u = u + uw;  verts[2].v = v + vh;
        verts[3].u = u;       verts[3].v = v + vh;
        for (int i = 0; i < 4; i++) {
            verts[i].r = r; verts[i].g = g;
            verts[i].b = b; verts[i].a = a;
        }
    }

    g_sprite_count++;
}

void aurora_sprite2d_clear() {
    g_sprite_count = 0;
}

void aurora_sprite2d_flush() {
    if (!g_initialized || g_sprite_count == 0) return;

    glUseProgram_fn(g_program);
    glUniform1i_fn(g_tex_uniform, 0);

    /* Upload vertex data */
    glBindBuffer_fn(GL_ARRAY_BUFFER, g_vbo);
    glBufferSubData_fn(GL_ARRAY_BUFFER, 0,
                       (GLsizeiptr)g_sprite_count * 4 * sizeof(SpriteVertex),
                       g_vertex_data);

    /* Simple batch render (all sprites with same texture) */
    glBindVertexArray_fn(g_vao);

    /* Group by texture */
    int start = 0;
    while (start < g_sprite_count) {
        GLuint cur_tex = g_texture_ids[start];
        int end = start + 1;
        while (end < g_sprite_count && g_texture_ids[end] == cur_tex) end++;

        glBindTexture_fn(GL_TEXTURE_2D, cur_tex);
        glDrawElements_fn(GL_TRIANGLES, (end - start) * 6,
                          GL_UNSIGNED_INT, (void*)(uintptr_t)(start * 6 * sizeof(GLuint)));

        start = end;
    }

    glBindVertexArray_fn(0);
}

void aurora_sprite2d_delete_texture(int id) {
    if (id <= 0 || id >= g_texture_count) return; /* can't delete default white */
    GLuint gl_id = g_textures[id].gl_id;
    glDeleteTextures_fn(1, &gl_id);
    /* Compact registry */
    for (int i = id; i < g_texture_count - 1; i++) {
        g_textures[i] = g_textures[i + 1];
    }
    g_texture_count--;
}

void aurora_sprite2d_shutdown() {
    if (!g_initialized) return;
    if (g_program) glDeleteProgram_fn(g_program);
    if (g_vao) glDeleteVertexArrays_fn(1, &g_vao);
    if (g_vbo) glDeleteBuffers_fn(1, &g_vbo);
    if (g_ibo) glDeleteBuffers_fn(1, &g_ibo);
    for (int i = 0; i < g_texture_count; i++) {
        if (g_textures[i].gl_id) glDeleteTextures_fn(1, &g_textures[i].gl_id);
    }
    free(g_vertex_data);
    free(g_texture_ids);
    g_vertex_data = nullptr;
    g_texture_ids = nullptr;
    g_sprite_count = 0;
    g_max_sprites = 0;
    g_texture_count = 0;
    g_initialized = 0;
}

} /* extern "C" */
