#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque types ── */
typedef struct AuroraTemplate AuroraTemplate;

/* ── Compile and register a template by name ── */
AuroraTemplate* aurora_template_compile(const char* name, const char* source);

/* ── Render a compiled template with JSON context ── */
char* aurora_template_render(const char* name, const char* context_json);

/* ── Render to string (caller frees with aurora_free) ── */
char* aurora_template_render_to_string(const char* name, const char* context_json);

/* ── Free a compiled template ── */
void aurora_template_free(const char* name);

/* ── Register template from string (convenience) ── */
AuroraTemplate* aurora_template_register_string(const char* name, const char* source);

/* ── Render directly from source string (no caching) ── */
char* aurora_template_render_string(const char* source, const char* context_json);

/* ── Compile and register a template from file (supports auto-reload) ── */
int aurora_template_compile_from_file(const char* name, const char* filepath);

#ifdef __cplusplus
}
#endif
