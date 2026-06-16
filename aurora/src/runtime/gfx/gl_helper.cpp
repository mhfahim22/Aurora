/* ── GL helper: wraps tricky OpenGL C APIs for Aurora FFI ──
 * On Windows, links opengl32.lib and loads function pointers dynamically.
 * On Linux/macOS, uses OpenGL framework / libGL.
 *
 * All functions are extern "C" for Aurora JIT resolution.
 */

#include <cstdint>
#include <cstdlib>
#include <cstring>

/* ── Platform GL types (no GL headers needed) ── */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
typedef unsigned int  GLuint;
typedef unsigned int  GLenum;
typedef int           GLint;
typedef int           GLsizei;
typedef char          GLchar;
#define APIENTRY __stdcall
#else
#include <dlfcn.h>
/* Use actual GL types from system headers if available, otherwise define our own */
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#endif

extern "C" {

/* ── Function pointer loading ── */
#ifdef _WIN32
static void* gl_load(const char* name) {
    HMODULE gl = LoadLibraryA("opengl32.dll");
    /* Try GetProcAddress first (handles forwarded exports on modern Windows) */
    void* ptr = (void*)GetProcAddress(gl, name);
    if (!ptr) {
        /* Fallback to wglGetProcAddress for extension functions */
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
    /* On Linux/macOS, GL functions are available via dlsym(RTLD_DEFAULT) */
    return dlsym(RTLD_DEFAULT, name);
}
#endif

/* ── Function pointer types for modern GL ── */
typedef void     (*FP_ShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*);
typedef void     (*FP_GetShaderiv)(GLuint, GLenum, GLint*);
typedef void     (*FP_GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void     (*FP_GetProgramiv)(GLuint, GLenum, GLint*);
typedef void     (*FP_GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLuint   (*FP_CreateShader)(GLenum);
typedef GLuint   (*FP_CreateProgram)();
typedef void     (*FP_CompileShader)(GLuint);
typedef void     (*FP_AttachShader)(GLuint, GLuint);
typedef void     (*FP_LinkProgram)(GLuint);
typedef void     (*FP_UseProgram)(GLuint);
typedef void     (*FP_DeleteShader)(GLuint);
typedef void     (*FP_DeleteProgram)(GLuint);

static FP_ShaderSource      glShaderSource_fn      = nullptr;
static FP_GetShaderiv       glGetShaderiv_fn       = nullptr;
static FP_GetShaderInfoLog  glGetShaderInfoLog_fn  = nullptr;
static FP_GetProgramiv      glGetProgramiv_fn      = nullptr;
static FP_GetProgramInfoLog glGetProgramInfoLog_fn = nullptr;
static FP_CreateShader      glCreateShader_fn      = nullptr;
static FP_CreateProgram     glCreateProgram_fn     = nullptr;
static FP_CompileShader     glCompileShader_fn     = nullptr;
static FP_AttachShader      glAttachShader_fn      = nullptr;
static FP_LinkProgram       glLinkProgram_fn       = nullptr;
static FP_UseProgram        glUseProgram_fn        = nullptr;
static FP_DeleteShader      glDeleteShader_fn      = nullptr;
static FP_DeleteProgram     glDeleteProgram_fn     = nullptr;

static int gl_loaded = 0;
static void gl_ensure_loaded() {
    if (gl_loaded) return;
    gl_loaded = 1;
    glShaderSource_fn      = (FP_ShaderSource)     gl_load("glShaderSource");
    glGetShaderiv_fn       = (FP_GetShaderiv)      gl_load("glGetShaderiv");
    glGetShaderInfoLog_fn  = (FP_GetShaderInfoLog) gl_load("glGetShaderInfoLog");
    glGetProgramiv_fn      = (FP_GetProgramiv)     gl_load("glGetProgramiv");
    glGetProgramInfoLog_fn = (FP_GetProgramInfoLog)gl_load("glGetProgramInfoLog");
    glCreateShader_fn      = (FP_CreateShader)     gl_load("glCreateShader");
    glCreateProgram_fn     = (FP_CreateProgram)    gl_load("glCreateProgram");
    glCompileShader_fn     = (FP_CompileShader)    gl_load("glCompileShader");
    glAttachShader_fn      = (FP_AttachShader)     gl_load("glAttachShader");
    glLinkProgram_fn       = (FP_LinkProgram)      gl_load("glLinkProgram");
    glUseProgram_fn        = (FP_UseProgram)       gl_load("glUseProgram");
    glDeleteShader_fn      = (FP_DeleteShader)     gl_load("glDeleteShader");
    glDeleteProgram_fn     = (FP_DeleteProgram)    gl_load("glDeleteProgram");
}

/* ── Public wrapper API ── */

void aurora_gl_shader_source(GLuint shader, const char* source) {
    gl_ensure_loaded();
    if (glShaderSource_fn)
        glShaderSource_fn(shader, 1, (const GLchar**)&source, nullptr);
}

void aurora_gl_compile_shader(GLuint shader) {
    gl_ensure_loaded();
    if (glCompileShader_fn) glCompileShader_fn(shader);
}

GLuint aurora_gl_create_shader(GLenum type) {
    gl_ensure_loaded();
    return glCreateShader_fn ? glCreateShader_fn(type) : 0;
}

GLuint aurora_gl_create_program() {
    gl_ensure_loaded();
    return glCreateProgram_fn ? glCreateProgram_fn() : 0;
}

void aurora_gl_attach_shader(GLuint program, GLuint shader) {
    gl_ensure_loaded();
    if (glAttachShader_fn) glAttachShader_fn(program, shader);
}

void aurora_gl_link_program(GLuint program) {
    gl_ensure_loaded();
    if (glLinkProgram_fn) glLinkProgram_fn(program);
}

void aurora_gl_use_program(GLuint program) {
    gl_ensure_loaded();
    if (glUseProgram_fn) glUseProgram_fn(program);
}

void aurora_gl_delete_shader(GLuint shader) {
    gl_ensure_loaded();
    if (glDeleteShader_fn) glDeleteShader_fn(shader);
}

void aurora_gl_delete_program(GLuint program) {
    gl_ensure_loaded();
    if (glDeleteProgram_fn) glDeleteProgram_fn(program);
}

int aurora_gl_get_shader_iv(GLuint shader, GLenum pname) {
    gl_ensure_loaded();
    if (!glGetShaderiv_fn) return 0;
    GLint result = 0;
    glGetShaderiv_fn(shader, pname, &result);
    return (int)result;
}

int aurora_gl_get_program_iv(GLuint program, GLenum pname) {
    gl_ensure_loaded();
    if (!glGetProgramiv_fn) return 0;
    GLint result = 0;
    glGetProgramiv_fn(program, pname, &result);
    return (int)result;
}

char* aurora_gl_get_shader_info_log(GLuint shader) {
    gl_ensure_loaded();
    if (!glGetShaderInfoLog_fn) return nullptr;
    GLint len = 0;
    glGetShaderiv_fn(shader, 0x8B84, &len);
    char* buf = (char*)malloc((size_t)(len > 0 ? len : 1));
    if (!buf) return nullptr;
    if (len > 0) {
        GLsizei written = 0;
        glGetShaderInfoLog_fn(shader, len, &written, buf);
        buf[written] = 0;
    } else {
        buf[0] = 0;
    }
    return buf;
}

char* aurora_gl_get_program_info_log(GLuint program) {
    gl_ensure_loaded();
    if (!glGetProgramInfoLog_fn) return nullptr;
    GLint len = 0;
    glGetProgramiv_fn(program, 0x8B84, &len);
    char* buf = (char*)malloc((size_t)(len > 0 ? len : 1));
    if (!buf) return nullptr;
    if (len > 0) {
        GLsizei written = 0;
        glGetProgramInfoLog_fn(program, len, &written, buf);
        buf[written] = 0;
    } else {
        buf[0] = 0;
    }
    return buf;
}

GLuint aurora_gl_compile_program(const char* vert_src, const char* frag_src) {
    gl_ensure_loaded();
    if (!glCreateShader_fn || !glCreateProgram_fn) return 0;

    GLuint vs = glCreateShader_fn(0x8B31);
    if (!vs) return 0;
    glShaderSource_fn(vs, 1, (const GLchar**)&vert_src, nullptr);
    glCompileShader_fn(vs);
    GLint vs_ok = 0;
    glGetShaderiv_fn(vs, 0x8B81, &vs_ok);
    if (!vs_ok) { glDeleteShader_fn(vs); return 0; }

    GLuint fs = glCreateShader_fn(0x8B30);
    if (!fs) { glDeleteShader_fn(vs); return 0; }
    glShaderSource_fn(fs, 1, (const GLchar**)&frag_src, nullptr);
    glCompileShader_fn(fs);
    GLint fs_ok = 0;
    glGetShaderiv_fn(fs, 0x8B81, &fs_ok);
    if (!fs_ok) { glDeleteShader_fn(vs); glDeleteShader_fn(fs); return 0; }

    GLuint prog = glCreateProgram_fn();
    if (!prog) { glDeleteShader_fn(vs); glDeleteShader_fn(fs); return 0; }
    glAttachShader_fn(prog, vs);
    glAttachShader_fn(prog, fs);
    glLinkProgram_fn(prog);
    GLint link_ok = 0;
    glGetProgramiv_fn(prog, 0x8B82, &link_ok);

    glDeleteShader_fn(vs);
    glDeleteShader_fn(fs);

    if (!link_ok) { glDeleteProgram_fn(prog); return 0; }
    return prog;
}

void aurora_gl_free_string(char* s) {
    free(s);
}

} /* extern "C" */
