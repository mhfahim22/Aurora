#pragma once
#include <string>
#include <vector>

struct JsonValue;

/* ── Bridge npm helper declarations (shared across split files) ── */

bool npm_has_native_addon(const JsonValue& json);
const char* npm_cjs_fallback(const std::string& pkg);
std::string sanitize_ident(const std::string& name);
void run_esbuild_transpile(const std::string& entry_path);
