#include "bridge_shared.h"
#include <set>

/* Forward declarations */
static void parse_cfg_features(const std::string& attr, std::set<std::string>& out,
                                bool* had_filtered);
enum RetCap { RET_SERDE, RET_DISPLAY, RET_HANDLE, RET_UNKNOWN };
enum ArgCap { ARG_SERDE, ARG_FROMSTR, ARG_HANDLE, ARG_UNKNOWN };
static RetCap return_strategy(const std::string& t);
static ArgCap arg_strategy(const std::string& t);
static bool is_non_serializable_type(const std::string& t);
static bool check_skip_due_to_platform_cfg(const std::string& attr_body);
static bool is_test_cfg(const std::string& attr_body);

/* ── Known concrete type substitutions per crate ─────────────
   Maps (crate, generic_base_type) → concrete type params to use
   instead of () placeholder. This allows bounded-generic types
   like Map<K: Ord, V> to be resolved as Map<String, Value>. */
struct ConcreteSubst {
    const char* crate_name;
    const char* base_type;
    const char* concrete_params;  /* e.g. "String, Value" */
};

static const ConcreteSubst known_concrete_types[] = {
    /* serde_json: Map<K: Ord, V> → Map<String, Value> */
    {"serde_json", "Map", "String, serde_json::Value"},
    {"serde_json", "HashMap", "String, serde_json::Value"},
    {"serde_json", "BTreeMap", "String, serde_json::Value"},
    {"serde_json", "IndexMap", "String, serde_json::Value"},
    {"serde_json", "Deserializer", "serde_json::Value"},
    {"serde_json", "StreamDeserializer", "serde_json::Value, serde_json::Value"},
    {"serde_json", "LineColIterator", "String"},
    /* serde: Serializer<...> is trait-level, skip */
};

static std::string lookup_concrete_type(const std::string& pkg,
                                          const std::string& base_type) {
    for (const auto& ct : known_concrete_types) {
        if (pkg == ct.crate_name && base_type == ct.base_type)
            return ct.concrete_params;
    }
    return {};
}
/* ── gen_cargo_au_binding ─────────────────────────────────────
   Generates the .au binding file for a Cargo crate, exposing
   standard FFI entry points plus type-instance method entries.
   ────────────────────────────────────────────────────────── */
void gen_cargo_au_binding(const std::string& pkg, const JsonValue& /*json*/,
                           const std::string& ver, std::ostream& os,
                           const std::string& extra_au) {
    os << "/* " << std::string(50, '=') << "\n"
       << "   Cargo Bridge " << char(151) << " Auto-generated Aurora FFI Bindings\n"
       << "   Package: " << pkg << "@" << ver << "\n"
       << "   " << std::string(50, '=') << " */\n\n"
       << "/* Module init */\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_import() -> pointer\n\n"
       << "/* FFI entry points */\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call(mod: pointer, fn: cstring, args: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call1(mod: pointer, fn: cstring, arg: pointer) -> pointer\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_free(handle: pointer)\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_free_cstr(handle: pointer)\n\n"
       << "/* Value conversion helpers */\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_str(s: cstring) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_int(v: i64) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_float(v: f64) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call_kw(mod: pointer, fn: cstring, args: pointer, kwargs: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_getattr(obj: pointer, name: cstring) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call2(mod: pointer, fn: cstring, arg1: pointer, arg2: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call3(mod: pointer, fn: cstring, arg1: pointer, arg2: pointer, arg3: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call4(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call5(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer, a5: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call6(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer, a5: pointer, a6: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_list(items: pointer, count: i32) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_tuple(items: pointer, count: i32) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_tuple2(a: pointer, b: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_tuple3(a: pointer, b: pointer, c: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_tuple4(a: pointer, b: pointer, c: pointer, d: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_list2(a: pointer, b: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_list3(a: pointer, b: pointer, c: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_list4(a: pointer, b: pointer, c: pointer, d: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_to_cstr(obj: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_dict() -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_dict_set(d: pointer, key: cstring, val: pointer) -> i32\n\n"
       << "/* Type instance API " << char(151) << " construct, call method, destroy */\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_construct(type_name: cstring, ctor: cstring, args: pointer) -> pointer\n"
       << "@cost(alloc)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_call_method(handle: pointer, type_name: cstring, method: cstring, args: pointer) -> pointer\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_free_type(handle: pointer, type_name: cstring)\n"
       << "@cost(zero)\n"
       << "extern \"cargo_" << pkg << "\" function " << pkg << "_last_error() -> cstring\n\n";

    if (!extra_au.empty()) {
        os << "/* Type instance entries (auto-discovered) */\n"
           << extra_au << "\n";
    }

    os << "/* Usage:\n"
       << "     import \"" << pkg << "\"\n"
       << "     let mod = " << pkg << "_import()\n"
       << "     let args = " << pkg << "_tuple2(a, b)\n"
       << "     let result = " << pkg << "_call(mod, \"fn_name\", args)\n"
       << "     " << pkg << "_free(result)\n"
       << "*/\n";
}

static bool is_test_cfg(const std::string& attr_body) {
    if (attr_body.find("not(") != std::string::npos) return false;
    return (attr_body == "cfg(test)" ||
            attr_body.find("(test)") != std::string::npos ||
            attr_body.find("(test,") != std::string::npos ||
            attr_body.find(",test,") != std::string::npos ||
            attr_body.find(",test)") != std::string::npos ||
            attr_body == "test");
}

static bool parse_rust_fn(const std::string& content, size_t& offset,
                           std::string& name, std::string& args_str,
                           std::string& return_type,
                           bool& has_self, bool& is_async,
                           bool& has_generics,
                           std::set<std::string>* cfg_features = nullptr,
                           bool* had_filtered_features = nullptr,
                           bool* skip_platform = nullptr)
{
    /* Skip attributes (#[...]) and doc comments (///, //!) */
    size_t skip_start = offset;
    while (skip_start < content.size()) {
        /* Skip whitespace and newlines */
        while (skip_start < content.size() && (content[skip_start] == ' ' || content[skip_start] == '\t' ||
               content[skip_start] == '\n' || content[skip_start] == '\r')) skip_start++;
        /* Skip line doc comment /// or //! */
        if (skip_start + 2 < content.size() && content[skip_start] == '/' && content[skip_start+1] == '/' &&
            (content[skip_start+2] == '/' || content[skip_start+2] == '!')) {
            skip_start += 3;
            while (skip_start < content.size() && content[skip_start] != '\n') skip_start++;
            continue;
        }
        /* Skip block doc comment /** ... */
        if (skip_start + 2 < content.size() && content[skip_start] == '/' && content[skip_start+1] == '*' &&
            content[skip_start+2] == '*') {
            skip_start += 3;
            while (skip_start + 1 < content.size() && !(content[skip_start] == '*' && content[skip_start+1] == '/')) skip_start++;
            if (skip_start + 1 < content.size()) skip_start += 2;
            continue;
        }
        /* Skip block comment /* ... */
        if (skip_start + 1 < content.size() && content[skip_start] == '/' && content[skip_start+1] == '*') {
            skip_start += 2;
            while (skip_start + 1 < content.size() && !(content[skip_start] == '*' && content[skip_start+1] == '/')) skip_start++;
            if (skip_start + 1 < content.size()) skip_start += 2;
            continue;
        }
        /* Skip line comment // */
        if (skip_start + 1 < content.size() && content[skip_start] == '/' && content[skip_start+1] == '/') {
            skip_start += 2;
            while (skip_start < content.size() && content[skip_start] != '\n') skip_start++;
            continue;
        }
        /* Skip attributes #[...] — track depth for nested #[attr] */
        if (skip_start < content.size() && content[skip_start] == '#') {
            size_t attr_pos = skip_start;
            skip_start++;
            if (skip_start < content.size() && content[skip_start] == '!') skip_start++; /* #![...] */
            if (skip_start < content.size() && content[skip_start] == '[') {
                int depth = 1;
                size_t bracket_content_start = skip_start + 1;
                skip_start++;
                while (skip_start < content.size() && depth > 0) {
                    if (content[skip_start] == '[') depth++;
                    else if (content[skip_start] == ']') depth--;
                    skip_start++;
                }
                /* Extract cfg features if requested */
                {
                    std::string attr_body = content.substr(bracket_content_start,
                        skip_start - bracket_content_start - 1);
                    if (cfg_features)
                        parse_cfg_features(attr_body, *cfg_features, had_filtered_features);
                    if (skip_platform && check_skip_due_to_platform_cfg(attr_body))
                        *skip_platform = true;
                }
                continue;
            }
            /* Not an attribute, restore position */
            skip_start = attr_pos;
        }
        break;
    }


    /* Scan linearly from skip_start for "pub ... fn", stopping at "impl" */
    size_t pub_pos = content.size();
    size_t fn_name_end = 0;
    bool found_async = false;
    {
    /* Check if skip_start is inside a // comment line — if so, advance past it */
    {
        size_t back = skip_start;
        while (back > 0 && content[back-1] != '\n') back--;
        size_t ck = back;
        while (ck + 1 < content.size() && ck < skip_start) {
            if (content[ck] == '/' && content[ck+1] == '/') {
                size_t nl = skip_start;
                while (nl < content.size() && content[nl] != '\n') nl++;
                skip_start = (nl < content.size()) ? nl + 1 : nl;
                goto start_scan;
            }
            ck++;
        }
        /* Check if we're at the first / of a // comment */
        if (skip_start + 1 < content.size() && content[skip_start] == '/' && content[skip_start+1] == '/') {
            size_t nl = skip_start;
            while (nl < content.size() && content[nl] != '\n') nl++;
            skip_start = (nl < content.size()) ? nl + 1 : nl;
            goto start_scan;
        }
        /* Check if we're past // but still on the same line (immediate backward check) */
        if (skip_start >= 2 && content[skip_start-2] == '/' && content[skip_start-1] == '/') {
            size_t nl = skip_start;
            while (nl < content.size() && content[nl] != '\n') nl++;
            skip_start = (nl < content.size()) ? nl + 1 : nl;
            goto start_scan;
        }
    }
    start_scan:
    size_t pos = skip_start;
        while (pos < content.size()) {
            char c = content[pos];
            if (isspace((unsigned char)c)) { pos++; continue; }
            /* Check for impl → stop scanning, let main loop detect it */
            if (pos + 4 < content.size() && content[pos] == 'i' && content[pos+1] == 'm' &&
                content[pos+2] == 'p' && content[pos+3] == 'l' &&
                (pos + 4 >= content.size() || !isalnum((unsigned char)content[pos+4]))) {
                return false;
            }
            /* Skip line comment // */
            if (pos + 1 < content.size() && c == '/' && content[pos+1] == '/') {
                pos += 2; while (pos < content.size() && content[pos] != '\n') pos++;
                continue;
            }
            /* Skip block comment /* */
            if (pos + 1 < content.size() && c == '/' && content[pos+1] == '*') {
                pos += 2; while (pos + 1 < content.size() && !(content[pos] == '*' && content[pos+1] == '/')) pos++;
                if (pos < content.size()) pos++;
                continue;
            }
            /* Skip attributes #[...] */
            if (c == '#') {
                size_t attr_pos = pos;
                pos++;
                if (pos < content.size() && content[pos] == '!') pos++;
                if (pos < content.size() && content[pos] == '[') {
                    int depth = 1; pos++;
                    while (pos < content.size() && depth > 0) {
                        if (content[pos] == '[') depth++;
                        else if (content[pos] == ']') depth--;
                        pos++;
                    }
                    if (depth == 0) {
                        /* Extract cfg features */
                        std::string attr_body = content.substr(attr_pos + (content[attr_pos+1]=='!'?3:2), pos - attr_pos - (content[attr_pos+1]=='!'?4:3));
                        if (cfg_features) parse_cfg_features(attr_body, *cfg_features, had_filtered_features);
                        if (skip_platform && check_skip_due_to_platform_cfg(attr_body)) *skip_platform = true;
                        continue;
                    }
                }
                pos = attr_pos + 1;
                continue;
            }
            /* Check for "pub" */
            if (pos + 3 < content.size() && c == 'p' && content[pos+1] == 'u' && content[pos+2] == 'b') {
                size_t p = pos + 3;
                while (p < content.size() && isspace((unsigned char)content[p])) p++;
                size_t mod_start = p;
                while (p < content.size()) {
                    if (p + 5 < content.size() && content.substr(p, 5) == "unsafe") { p += 5; goto mod_skip; }
                    if (p + 5 < content.size() && content.substr(p, 5) == "async") { p += 5; found_async = true; goto mod_skip; }
                    if (p + 6 < content.size() && content.substr(p, 6) == "extern") {
                        p += 6;
                        while (p < content.size() && isspace((unsigned char)content[p])) p++;
                        if (p < content.size() && content[p] == '"') {
                            p++; while (p < content.size() && content[p] != '"') p++;
                            if (p < content.size()) p++;
                        }
                        goto mod_skip;
                    }
                    break;
                mod_skip:
                    while (p < content.size() && isspace((unsigned char)content[p])) p++;
                }
                if (p + 1 < content.size() && content[p] == 'f' && content[p+1] == 'n') {
                    pub_pos = pos;
                    p += 2;
                    while (p < content.size() && isspace((unsigned char)content[p])) p++;
                    size_t name_start = p;
                    while (p < content.size() && (isalnum((unsigned char)content[p]) || content[p] == '_')) p++;
                    if (p > name_start) {
                        name = content.substr(name_start, p - name_start);
                        fn_name_end = p;
                        is_async = found_async;
                        break;
                    }
                }
                /* Not pub fn — skip past this pub declaration (struct/enum/trait/union) */
                {
                    size_t pp = mod_start;
                    /* Skip to first '{' or ';' (past item name, generics, where clause) */
                    int paren_depth = 0, angle_depth = 0;
                    bool in_str = false;
                    while (pp < content.size()) {
                        char cp = content[pp];
                        if (in_str) {
                            if (cp == '\\') { pp += 2; continue; }
                            if (cp == '"') in_str = false;
                            pp++; continue;
                        }
                        if (cp == '"') { in_str = true; pp++; continue; }
                        if (cp == '(') { paren_depth++; pp++; continue; }
                        if (cp == ')') { paren_depth--; pp++; continue; }
                        if (cp == '<') { angle_depth++; pp++; continue; }
                        if (cp == '>') { angle_depth--; pp++; continue; }
                        if (cp == '/' && pp + 1 < content.size()) {
                            if (content[pp+1] == '/') { pp += 2; while (pp < content.size() && content[pp] != '\n') pp++; continue; }
                            if (content[pp+1] == '*') { pp += 2; while (pp + 1 < content.size() && !(content[pp] == '*' && content[pp+1] == '/')) pp++; pp += 2; continue; }
                        }
                        if (cp == '{' && paren_depth == 0 && angle_depth == 0) break;
                        if (cp == ';' && paren_depth == 0 && angle_depth == 0) break;
                        pp++;
                    }
                    /* Consume { ... } or ; */
                    if (pp < content.size() && content[pp] == '{') {
                        int bd = 1; pp++;
                        while (pp < content.size() && bd > 0) {
                            if (content[pp] == '"') { pp++; while (pp < content.size() && content[pp] != '"') { if (content[pp] == '\\') pp++; pp++; } if (pp < content.size()) pp++; continue; }
                            if (content[pp] == '{') bd++;
                            else if (content[pp] == '}') bd--;
                            pp++;
                        }
                    } else if (pp < content.size() && content[pp] == ';') {
                        pp++;
                    }
                    pos = pp;
                    continue;
                }
            }
            /* Non-pub keyword — try to skip past it */
            {
                size_t kw_end = pos;
                while (kw_end < content.size() && (isalnum((unsigned char)content[kw_end]) || content[kw_end] == '_')) kw_end++;
                std::string kw = content.substr(pos, kw_end - pos);
                if (kw == "fn" || kw == "struct" || kw == "enum" || kw == "union" ||
                    kw == "trait" || kw == "mod" || kw == "use" || kw == "type" ||
                    kw == "const" || kw == "static" || kw == "let" || kw == "macro") {
                    size_t pp = kw_end;
                    /* Skip to first '{' or ';' */
                    int paren_depth = 0, angle_depth = 0;
                    bool in_str = false;
                    while (pp < content.size()) {
                        char cp = content[pp];
                        if (in_str) {
                            if (cp == '\\') { pp += 2; continue; }
                            if (cp == '"') in_str = false;
                            pp++; continue;
                        }
                        if (cp == '"') { in_str = true; pp++; continue; }
                        if (cp == '(') { paren_depth++; pp++; continue; }
                        if (cp == ')') { paren_depth--; pp++; continue; }
                        if (cp == '<') { angle_depth++; pp++; continue; }
                        if (cp == '>') { angle_depth--; pp++; continue; }
                        if (cp == '/' && pp + 1 < content.size()) {
                            if (content[pp+1] == '/') { pp += 2; while (pp < content.size() && content[pp] != '\n') pp++; continue; }
                            if (content[pp+1] == '*') { pp += 2; while (pp + 1 < content.size() && !(content[pp] == '*' && content[pp+1] == '/')) pp++; pp += 2; continue; }
                        }
                        if (cp == '{' && paren_depth == 0 && angle_depth == 0) break;
                        if (cp == ';' && paren_depth == 0 && angle_depth == 0) break;
                        pp++;
                    }
                    /* Consume { ... } or ; */
                    if (pp < content.size() && content[pp] == '{') {
                        int bd = 1; pp++;
                        while (pp < content.size() && bd > 0) {
                            if (content[pp] == '"') { pp++; while (pp < content.size() && content[pp] != '"') { if (content[pp] == '\\') pp++; pp++; } if (pp < content.size()) pp++; continue; }
                            if (content[pp] == '{') bd++;
                            else if (content[pp] == '}') bd--;
                            pp++;
                        }
                    } else if (pp < content.size() && content[pp] == ';') {
                        pp++;
                    }
                    pos = pp;
                    continue;
                }
                /* Unknown identifier/non-word char — advance by 1 */
                pos++;
                continue;
            }
        }
    }
    if (pub_pos >= content.size()) return false;

    has_generics = false;
    size_t fn_end = fn_name_end;

    /* Skip whitespace and optional generics <...> before '(' */
    size_t pos = fn_end;
    int angle_depth = 0;
    while (pos < content.size()) {
        char c = content[pos];
        if (c == '<') { angle_depth++; has_generics = true; pos++; }
        else if (c == '>') { if (angle_depth > 0) angle_depth--; else break; pos++; }
        else if (c == '(' && angle_depth == 0) break;
        else if (c == '(') { pos++; } /* inside generics */
        else if (angle_depth > 0) { pos++; } /* skip any char inside generics */
        else if (isspace((unsigned char)c)) { pos++; }
        else break;
    }
    if (pos >= content.size() || content[pos] != '(') return false;

    /* Scan forward tracking depth to find matching ')' */
    size_t paren_start = pos + 1;
    int paren_depth = 1;
    pos = paren_start;
    bool in_str = false;
    while (pos < content.size() && paren_depth > 0) {
        char c = content[pos];
        if (in_str) {
            if (c == '\\') { pos += 2; continue; }
            if (c == '"') in_str = false;
        } else {
            if (c == '"') in_str = true;
            else if (c == '(') paren_depth++;
            else if (c == ')') paren_depth--;
            else if (c == '/' && pos + 1 < content.size()) {
                if (content[pos+1] == '/') {
                    pos += 2; while (pos < content.size() && content[pos] != '\n') pos++;
                    continue;
                } else if (content[pos+1] == '*') {
                    pos += 2; while (pos + 1 < content.size() && !(content[pos] == '*' && content[pos+1] == '/')) pos++;
                    pos += 2; continue;
                }
            }
        }
        pos++;
    }
    if (paren_depth != 0) return false;

    args_str = content.substr(paren_start, pos - paren_start - 1);
    offset = pos; /* update offset past the function signature */

    /* Check for &self, self, mut self in the args */
    has_self = (args_str.find("self") != std::string::npos);

    /* ── Parse return type after -> ── */
    return_type.clear();
    size_t rt_pos = offset;
    /* Skip whitespace after ')' */
    while (rt_pos < content.size() && isspace((unsigned char)content[rt_pos])) rt_pos++;
    /* Check for -> */
    if (rt_pos + 1 < content.size() && content[rt_pos] == '-' && content[rt_pos+1] == '>') {
        rt_pos += 2;
        while (rt_pos < content.size() && isspace((unsigned char)content[rt_pos])) rt_pos++;
        size_t rt_start = rt_pos;
        int angle_depth = 0, paren_depth = 0, bracket_depth = 0;
        bool in_str = false;
        while (rt_pos < content.size()) {
            char c = content[rt_pos];
            if (in_str) {
                if (c == '\\') { rt_pos += 2; continue; }
                if (c == '"') in_str = false;
                rt_pos++; continue;
            }
            if (c == '"') { in_str = true; rt_pos++; continue; }
            /* Comment skipping */
            if (c == '/' && rt_pos + 1 < content.size()) {
                if (content[rt_pos+1] == '/') {
                    rt_pos += 2; while (rt_pos < content.size() && content[rt_pos] != '\n') rt_pos++;
                    continue;
                } else if (content[rt_pos+1] == '*') {
                    rt_pos += 2; while (rt_pos + 1 < content.size() && !(content[rt_pos] == '*' && content[rt_pos+1] == '/')) rt_pos++;
                    rt_pos += 2; continue;
                }
            }
            /* Terminators only at depth 0 */
            if (angle_depth == 0 && paren_depth == 0 && bracket_depth == 0) {
                if (c == '{' || c == ';' || c == ',') break;
                if (isspace((unsigned char)c)) {
                    /* Check for 'where' clause */
                    size_t tmp = rt_pos + 1;
                    while (tmp < content.size() && isspace((unsigned char)content[tmp])) tmp++;
                    if (tmp + 5 < content.size() && content.substr(tmp, 5) == "where") break;
                    /* Space within return type (e.g., "&'static mut [u8]") — keep going */
                    rt_pos++; continue;
                }
            }
            /* Depth tracking */
            if (c == '<') angle_depth++;
            else if (c == '>' && angle_depth > 0) angle_depth--;
            else if (c == '(') paren_depth++;
            else if (c == ')' && paren_depth > 0) paren_depth--;
            else if (c == '[') bracket_depth++;
            else if (c == ']' && bracket_depth > 0) bracket_depth--;
            rt_pos++;
        }
        return_type = content.substr(rt_start, rt_pos - rt_start);
        /* Trim trailing whitespace */
        while (!return_type.empty() && isspace((unsigned char)return_type.back()))
            return_type.pop_back();
        offset = rt_pos;
    }

    return true;
}

/* ── Helper: escape a string for use in a Rust string literal ── */
static std::string rust_escape(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': r += "\\\\"; break;
            case '"':  r += "\\\""; break;
            case '\n': r += "\\n"; break;
            case '\t': r += "\\t"; break;
            case '\r': r += "\\r"; break;
            default:   r += c;
        }
    }
    return r;
}

/* ── Count real positional args from an args_str (— self, — generics depth) ── */
static int count_positional_args(const std::string& args_str) {
    int depth = 0, raw_args = 0;
    bool has_content = false, in_string = false;
    int last_comma_pos = -1;
    for (size_t i = 0; i < args_str.size(); i++) {
        char c = args_str[i];
        if (in_string) {
            if (c == '\\') { i++; continue; }
            if (c == '"') in_string = false;
            continue;
        }
        if (c == '"') { in_string = true; continue; }
        if (c == '<' || c == '(') depth++;
        else if (c == '>' || c == ')') depth--;
        else if (c == ',' && depth == 0) {
            raw_args++;
            last_comma_pos = (int)i;
        }
        if (!isspace((unsigned char)c) && c != ',' && depth == 0) has_content = true;
    }
    if (!has_content) return 0;
    /* Detect trailing comma: no non-whitespace content after last comma */
    if (last_comma_pos >= 0) {
        bool content_after = false;
        for (size_t i = (size_t)last_comma_pos + 1; i < args_str.size(); i++) {
            if (!isspace((unsigned char)args_str[i])) { content_after = true; break; }
        }
        if (!content_after) raw_args--; /* trailing comma, adjust */
    }
    return raw_args + 1;
}

/* ── Check if return type string indicates Result<T, E> ── */
    static std::string turbofish_type(const std::string& type_ref) {
        size_t ga = type_ref.find('<');
        if (ga == std::string::npos) return type_ref;
        return type_ref.substr(0, ga) + "::<" + type_ref.substr(ga + 1);
    }

    static bool is_result_type(const std::string& rt) {
    if (rt.empty()) return false;
    if (rt == "Result") return true;
    if (rt.rfind("Result<", 0) == 0) return true;
    if (rt.rfind("std::result::Result<", 0) == 0) return true;
    return false;
}

/* ── Check if a feature is unsafe to auto-enable ── */
/* These features should be skipped: they either require nightly, are pseudo-features,
   require unstable cfg flags, or conflict with other detected features */
static bool is_unsafe_feature(const std::string& feat) {
    return feat == "unstable" || feat == "nightly" || feat == "nightly-features"
        || feat == "cargo-clippy" || feat == "pattern"
        || feat == "simd_support" || feat == "avx512bw" || feat == "avx512"
        || feat.rfind("simd", 0) == 0
        || feat == "kv" || feat.rfind("kv_", 0) == 0
        /* Rust nightly feature names — enabling these via cfg_attr requires nightly */
        || feat == "specialization" || feat == "may_dangle"
        || feat == "dropck_eyepatch" || feat == "untagged_unions"
        || feat == "debugger_visualizer"
        /* Tokio features that require --cfg tokio_unstable */
        || feat == "io-uring" || feat == "taskdump";
}

/* ── Platform-cfg evaluator ── */
/* Returns true if the cfg expression matches the current build platform */
static std::vector<std::string> split_cfg_args(const std::string& s) {
    std::vector<std::string> args;
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '(' || s[i] == '[' || s[i] == '<') depth++;
        else if (s[i] == ')' || s[i] == ']' || s[i] == '>') depth--;
        else if (s[i] == ',' && depth == 0) {
            std::string arg = s.substr(start, i - start);
            size_t a = 0; while (a < arg.size() && isspace((unsigned char)arg[a])) a++;
            size_t b = arg.size(); while (b > a && isspace((unsigned char)arg[b-1])) b--;
            if (b > a) args.push_back(arg.substr(a, b - a));
            start = i + 1;
        }
    }
    std::string last = s.substr(start);
    size_t a = 0; while (a < last.size() && isspace((unsigned char)last[a])) a++;
    size_t b = last.size(); while (b > a && isspace((unsigned char)last[b-1])) b--;
    if (b > a) args.push_back(last.substr(a, b - a));
    return args;
}

/* Forward declaration */
static bool evaluate_cfg(const std::string& expr);

static bool evaluate_simple_cfg(const std::string& s) {
    /* key = "value" form: cfg(target_os = "windows") */
    size_t eq = s.find(" = ");
    if (eq != std::string::npos) {
        std::string key = s.substr(0, eq);
        std::string val = s.substr(eq + 3);
        if (val.size() >= 2 && val[0] == '"' && val.back() == '"')
            val = val.substr(1, val.size() - 2);
        if (key == "target_os") {
#ifdef _WIN32
            return val == "windows";
#elif defined(__linux__)
            return val == "linux";
#elif defined(__APPLE__)
            return val == "macos";
#else
            return false;
#endif
        }
        if (key == "target_family") {
#ifdef _WIN32
            return val == "windows";
#else
            return val == "unix";
#endif
        }
        if (key == "target_env") {
#ifdef _WIN32
            return val == "msvc";
#else
            (void)val; return false;
#endif
        }
        if (key == "target_arch") {
#if defined(__x86_64__) || defined(_M_X64)
            return val == "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
            return val == "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
            return val == "x86";
#else
            return false;
#endif
        }
        if (key == "target_pointer_width") {
            return val == std::to_string(sizeof(void*) * 8);
        }
        if (key == "feature") {
            /* Feature cfg — these are handled by parse_cfg_features, not here */
            return true; /* Don't skip on feature cfgs (handled separately) */
        }
        /* Unknown key — conservative: assume non-match */
        return false;
    }
    /* Single identifier: cfg(windows), cfg(unix) */
#ifdef _WIN32
    if (s == "windows") return true;
    if (s == "unix") return false;
#else
    if (s == "windows") return false;
    if (s == "unix") return true;
#endif
    if (s == "test") return false; /* #[cfg(test)] — never in release bridge */
    if (s == "not_test") return true;
    /* Unknown identifier — conservative: don't skip */
    return true;
}

static bool evaluate_cfg(const std::string& expr) {
    size_t start = 0;
    while (start < expr.size() && isspace((unsigned char)expr[start])) start++;
    size_t end = expr.size();
    while (end > start && isspace((unsigned char)expr[end-1])) end--;
    if (start >= end) return true;
    std::string s = expr.substr(start, end - start);

    /* not(pred) */
    if (s.size() > 5 && s.rfind("not(", 0) == 0 && s.back() == ')') {
        return !evaluate_cfg(s.substr(4, s.size() - 5));
    }
    /* all(pred1, pred2, ...) */
    if (s.size() > 5 && s.rfind("all(", 0) == 0 && s.back() == ')') {
        auto preds = split_cfg_args(s.substr(4, s.size() - 5));
        for (const auto& p : preds) {
            if (!evaluate_cfg(p)) return false;
        }
        return !preds.empty();
    }
    /* any(pred1, pred2, ...) */
    if (s.size() > 5 && s.rfind("any(", 0) == 0 && s.back() == ')') {
        auto preds = split_cfg_args(s.substr(4, s.size() - 5));
        for (const auto& p : preds) {
            if (evaluate_cfg(p)) return true;
        }
        return false;
    }
    return evaluate_simple_cfg(s);
}

/* Returns true if the attribute string contains a non-matching platform cfg */
static bool check_skip_due_to_platform_cfg(const std::string& attr_body) {
    /* Only handle #[cfg(...)] — NOT #[cfg_attr(...)] (which doesn't gate items) */
    if (attr_body.rfind("cfg(", 0) == 0 && attr_body.back() == ')') {
        std::string expr = attr_body.substr(4, attr_body.size() - 5);
        return !evaluate_cfg(expr);
    }
    return false;
}

/* ── Extract feature names from a #[cfg(...)] or #![cfg_attr(...)] attribute string ── */
static void parse_cfg_features(const std::string& attr, std::set<std::string>& out,
                                bool* had_filtered = nullptr) {
    if (had_filtered) *had_filtered = false;
    std::regex feat_re(R"-(feature\s*=\s*"([^"]*)")-");
    auto begin = std::sregex_iterator(attr.begin(), attr.end(), feat_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string feat = (*it)[1].str();
        if (feat.empty()) continue;
        size_t mp = (size_t)(*it).position();
        size_t cs = (mp >= 5) ? mp - 5 : 0;
        std::string before = attr.substr(cs, mp - cs);
        if (before.find("not(") != std::string::npos) continue;
        if (is_unsafe_feature(feat)) {
            if (had_filtered) *had_filtered = true;
            continue;
        }
        out.insert(feat);
    }
}

/* ── Generate deserialization calls for `count` args into `s` ── */
/* If args_str is non-empty, parses argument types to handle references specially */
static void gen_deser_args(std::string& s, const std::string& indent, int count,
                           const std::string& args_str = "",
                           bool has_self = false,
                           const std::string& pkg = "") {
    /* Parse argument types from args_str if provided */
    /* Format: name: type, name: type, ... (possibly with &self, mut self) */
    std::vector<bool> is_ref(count, false);
    std::vector<ArgCap> arg_caps(count, ARG_SERDE);
    std::vector<std::string> arg_types(count);        /* full Rust type for each arg */
    std::vector<std::string> ref_owned_types(count);
    if (!args_str.empty() && count > 0) {
        std::string astr = args_str;
        /* Remove leading self/&self/mut self */
        {
            size_t sp = 0;
            while (sp < astr.size()) {
                while (sp < astr.size() && isspace((unsigned char)astr[sp])) sp++;
                if (sp + 5 <= astr.size() && astr.substr(sp, 5) == "&self") {
                    size_t sep = astr.find(',', sp);
                    if (sep == std::string::npos) break;
                    sp = sep + 1; continue;
                }
                if (sp + 4 <= astr.size() && astr.substr(sp, 4) == "self") {
                    size_t sep = astr.find(',', sp);
                    if (sep == std::string::npos) break;
                    sp = sep + 1; continue;
                }
                if (sp + 3 <= astr.size() && astr.substr(sp, 3) == "mut") {
                    sp += 3;
                    while (sp < astr.size() && isspace((unsigned char)astr[sp])) sp++;
                    if (sp + 4 <= astr.size() && astr.substr(sp, 4) == "self") {
                        size_t sep = astr.find(',', sp);
                        if (sep == std::string::npos) break;
                        sp = sep + 1; continue;
                    }
                    sp -= 3; break;
                }
                break;
            }
            if (sp > 0) astr = astr.substr(sp);
        }
        for (int i = 0; i < count && !astr.empty(); i++) {
            /* Find colon to get type */
            size_t colon = astr.find(':');
            if (colon == std::string::npos) break;
            size_t tstart = colon + 1;
            while (tstart < astr.size() && isspace((unsigned char)astr[tstart])) tstart++;
            /* Check if type starts with & and extract inner type */
            if (tstart < astr.size() && astr[tstart] == '&') {
                is_ref[i] = true;
            }
            /* Move past this arg to find its end */
            int paren_depth = 0, angle_depth = 0, bracket_depth = 0;
            bool in_str = false;
            size_t aend = tstart;
            while (aend < astr.size()) {
                char c = astr[aend];
                if (in_str) {
                    if (c == '\\') { aend += 2; continue; }
                    if (c == '"') in_str = false;
                    aend++; continue;
                }
                if (c == '"') { in_str = true; aend++; continue; }
                if (c == '(') { paren_depth++; aend++; continue; }
                if (c == ')') { paren_depth--; aend++; continue; }
                if (c == '<') { angle_depth++; aend++; continue; }
                if (c == '>') { angle_depth--; aend++; continue; }
                if (c == '[') { bracket_depth++; aend++; continue; }
                if (c == ']') { bracket_depth--; aend++; continue; }
                if (paren_depth == 0 && angle_depth == 0 && bracket_depth == 0 && c == ',') break;
                if (paren_depth == 0 && angle_depth == 0 && bracket_depth == 0 && c == ')') break;
                aend++;
            }
            if (aend < astr.size() && astr[aend] == ',') aend++;
            /* Extract the raw type name for strategy detection */
            {
                std::string full_type = astr.substr(tstart, aend - tstart);
                /* Trim trailing comma/whitespace */
                while (!full_type.empty() && (full_type.back() == ',' || isspace((unsigned char)full_type.back())))
                    full_type.pop_back();
                arg_types[i] = full_type;
                /* Detect strategy by stripping &, lifetime, mut */
                std::string clean_type = full_type;
                size_t ti = 0;
                while (ti < clean_type.size() && (clean_type[ti] == '&' || isspace((unsigned char)clean_type[ti]))) ti++;
                while (ti < clean_type.size() && clean_type[ti] == '\'') {
                    ti++;
                    while (ti < clean_type.size() && (isalnum((unsigned char)clean_type[ti]) || clean_type[ti] == '_')) ti++;
                }
                while (ti < clean_type.size() && isspace((unsigned char)clean_type[ti])) ti++;
                if (ti + 3 <= clean_type.size() && clean_type.substr(ti, 3) == "mut") { ti += 3; while (ti < clean_type.size() && isspace((unsigned char)clean_type[ti])) ti++; }
                if (ti + 5 <= clean_type.size() && clean_type.substr(ti, 5) == "const") { ti += 5; while (ti < clean_type.size() && isspace((unsigned char)clean_type[ti])) ti++; }
                std::string base_type = clean_type.substr(ti);
                while (!base_type.empty() && (isspace((unsigned char)base_type.back()) || base_type.back() == ',')) base_type.pop_back();
                arg_caps[i] = arg_strategy(base_type);
            }
            /* For ref args, extract inner (owned) type by stripping &, lifetime, mut, const */
            if (is_ref[i]) {
                std::string full_type = astr.substr(tstart, aend - tstart);
                while (!full_type.empty() && (full_type.back() == ',' || isspace((unsigned char)full_type.back())))
                    full_type.pop_back();
                size_t ti = 0;
                while (ti < full_type.size() && (full_type[ti] == '&' || isspace((unsigned char)full_type[ti]))) ti++;
                while (ti < full_type.size() && full_type[ti] == '\'') {
                    ti++;
                    while (ti < full_type.size() && (isalnum((unsigned char)full_type[ti]) || full_type[ti] == '_')) ti++;
                }
                while (ti < full_type.size() && isspace((unsigned char)full_type[ti])) ti++;
                if (ti + 3 <= full_type.size() && full_type.substr(ti, 3) == "mut") {
                    ti += 3;
                    while (ti < full_type.size() && isspace((unsigned char)full_type[ti])) ti++;
                } else if (ti + 5 <= full_type.size() && full_type.substr(ti, 5) == "const") {
                    ti += 5;
                    while (ti < full_type.size() && isspace((unsigned char)full_type[ti])) ti++;
                }
                std::string inner = full_type.substr(ti);
                while (!inner.empty() && (isspace((unsigned char)inner.back()) || inner.back() == ',')) inner.pop_back();
                if (!pkg.empty() && !inner.empty()
                    && inner != "Self"
                    && inner != "self" && inner != "bool"
                    && inner != "u8" && inner != "u16" && inner != "u32" && inner != "u64" && inner != "u128"
                    && inner != "i8" && inner != "i16" && inner != "i32" && inner != "i64" && inner != "i128"
                    && inner != "f32" && inner != "f64" && inner != "usize" && inner != "isize"
                    && inner != "char" && inner != "str" && inner != "String"
                    && inner.find("::") == std::string::npos
                    && inner[0] != '[' && inner.find('(') == std::string::npos
                    && inner.find('<') == std::string::npos
                    && inner.find('>') == std::string::npos) {
                    inner = pkg + "::" + inner;
                }
                if (inner.substr(0, 4) == "[u8]") {
                    ref_owned_types[i] = "Vec<u8>";
                } else if (inner == "str") {
                    ref_owned_types[i] = "String";
                } else if (inner.size() >= 2 && inner[0] == '[' && inner.back() == ']') {
                    std::string elem = inner.substr(1, inner.size() - 2);
                    if (elem.find(';') != std::string::npos) {
                        ref_owned_types[i] = inner;
                    } else {
                        ref_owned_types[i] = "Vec<" + elem + ">";
                    }
                } else {
                    ref_owned_types[i] = inner;
                }
            }
            astr = astr.substr(aend);
        }
    }

    for (int i = 0; i < count; i++) {
        s += indent + "let a" + std::to_string(i) + ": serde_json::Value = args.get(" + std::to_string(i)
           + ").ok_or_else(|| \"" + std::to_string(i) + ": missing arg\".to_string())?.clone();\n";
        if (i < (int)arg_caps.size() && arg_caps[i] == ARG_FROMSTR) {
            /* FromStr-based arg: parse from JSON string */
            std::string type_name = (i < (int)arg_types.size()) ? arg_types[i] : "String";
            /* Clean the type name for Rust code */
            std::string rust_type = type_name;
            size_t ci = 0;
            while (ci < rust_type.size() && (rust_type[ci] == '&' || isspace((unsigned char)rust_type[ci]))) ci++;
            while (ci < rust_type.size() && rust_type[ci] == '\'') {
                ci++;
                while (ci < rust_type.size() && (isalnum((unsigned char)rust_type[ci]) || rust_type[ci] == '_')) ci++;
            }
            while (ci < rust_type.size() && isspace((unsigned char)rust_type[ci])) ci++;
            if (ci + 3 <= rust_type.size() && rust_type.substr(ci, 3) == "mut") ci += 3;
            if (ci + 5 <= rust_type.size() && rust_type.substr(ci, 5) == "const") ci += 5;
            while (ci < rust_type.size() && isspace((unsigned char)rust_type[ci])) ci++;
            rust_type = rust_type.substr(ci);
            /* Map common types to their full qualified path */
            if (rust_type == "Duration" || rust_type == "std::time::Duration")
                rust_type = "std::time::Duration";
            else if (rust_type == "Uuid" || rust_type == "uuid::Uuid")
                rust_type = "uuid::Uuid";
            else if (rust_type == "Timestamp" || rust_type == "uuid::timestamp::Timestamp")
                rust_type = "uuid::timestamp::Timestamp";
            else if (rust_type == "Regex" || rust_type == "regex::Regex")
                rust_type = "regex::Regex";
            else if (rust_type == "Path" || rust_type == "PathBuf" || rust_type == "std::path::Path" || rust_type == "std::path::PathBuf")
                rust_type = "std::path::PathBuf";
            else if (rust_type == "SocketAddr" || rust_type == "std::net::SocketAddr")
                rust_type = "std::net::SocketAddr";
            else if (rust_type == "IpAddr" || rust_type == "std::net::IpAddr")
                rust_type = "std::net::IpAddr";
            else if (rust_type == "Url" || rust_type == "url::Url")
                rust_type = "url::Url";
            /* Handle ref args that need parse+reference (e.g. &Uuid) */
            if (i < (int)is_ref.size() && is_ref[i]) {
                s += indent + "let a" + std::to_string(i) + "_tmp: " + rust_type + " = a" + std::to_string(i)
                   + ".as_str().ok_or(\"arg " + std::to_string(i) + ": expected string\".to_string())?"
                   + ".parse().map_err(|e| format!(\"arg " + std::to_string(i) + " parse: {}\", e))?;\n";
                s += indent + "let a" + std::to_string(i) + " = &a" + std::to_string(i) + "_tmp;\n";
            } else {
                s += indent + "let a" + std::to_string(i) + ": " + rust_type + " = a" + std::to_string(i)
                   + ".as_str().ok_or(\"arg " + std::to_string(i) + ": expected string\".to_string())?"
                   + ".parse().map_err(|e| format!(\"arg " + std::to_string(i) + " parse: {}\", e))?;\n";
            }
        } else if (i < (int)is_ref.size() && is_ref[i]) {
            std::string otype = (i < (int)ref_owned_types.size() && !ref_owned_types[i].empty())
                ? ref_owned_types[i] : "serde_json::Value";
            if (otype == "serde_json::Value") {
                s += indent + "let a" + std::to_string(i) + " = &a" + std::to_string(i) + ";\n";
            } else {
                s += indent + "let a" + std::to_string(i) + "_owned: " + otype + " = __deser(a" + std::to_string(i) + ")?;\n";
                if (otype.size() >= 4 && otype.substr(0, 3) == "Vec") {
                    s += indent + "let a" + std::to_string(i) + " = &a" + std::to_string(i) + "_owned[..];\n";
                } else {
                    s += indent + "let a" + std::to_string(i) + " = &a" + std::to_string(i) + "_owned;\n";
                }
            }
        } else {
            s += indent + "let a" + std::to_string(i) + " = __deser(a" + std::to_string(i) + ")?;\n";
        }
    }
}

static bool is_raw_ptr_type(const std::string& rt) {
    if (rt.empty()) return false;
    size_t p = 0;
    while (p < rt.size() && isspace((unsigned char)rt[p])) p++;
    return (rt.compare(p, 5, "*mut ") == 0 || rt.compare(p, 7, "*const ") == 0);
}

    static bool is_self_ref_type(const std::string& rt) {
        if (rt.empty()) return false;
        std::string trimmed = rt;
        size_t p = 0;
        while (p < trimmed.size() && isspace((unsigned char)trimmed[p])) p++;
        trimmed = trimmed.substr(p);
        return trimmed == "Self" || trimmed == "&Self" || trimmed == "&mut Self";
    }

    static std::string placeholder_type(const std::string& impl_type) {
        size_t ga = impl_type.find('<');
        if (ga == std::string::npos) return {};
        std::string base = impl_type.substr(0, ga);
        while (!base.empty() && isspace((unsigned char)base.back())) base.pop_back();
        /* Collect non-lifetime, non-defaulted param names inside <...> */
        int depth = 0;
        std::vector<std::string> params;
        std::string current;
        bool after_comma = true;
        for (size_t i = ga + 1; i < impl_type.size() - 1; i++) {
            char c = impl_type[i];
            if (c == '<') { depth++; if (after_comma && depth == 1) { params.clear(); } continue; }
            if (c == '>') { depth--; if (depth < 0) break; continue; }
            if (depth > 0) continue;
            if (c == '\'') {
                while (i < impl_type.size() && impl_type[i] != ',' && impl_type[i] != '>') i++;
                if (i < impl_type.size() && impl_type[i] == ',') continue;
                break;
            }
            if (c == ',') {
                if (!current.empty() && current.find('=') == std::string::npos)
                    params.push_back(current);
                current.clear();
                after_comma = true;
                continue;
            }
            if (!isspace((unsigned char)c) || !current.empty())
                current += c;
            after_comma = false;
        }
        if (!current.empty() && current.find('=') == std::string::npos)
            params.push_back(current);
        if (params.empty()) return {};
        /* Types that require a trait bound (e.g. smallvec::Array) need a concrete type
           that satisfies the bound. SmallVec<A: Array> and IntoIter<A: Array> need a real
           array type; [u8; 8] implements the Array trait for any array size. */
        static const char* array_bound_types[] = {"SmallVec", "IntoIter", "DrainFilter"};
        bool needs_array_bound = false;
        for (auto* t : array_bound_types) {
            if (base == t) { needs_array_bound = true; break; }
        }
        std::string result = base + "<";
        for (size_t i = 0; i < params.size(); i++) {
            if (i > 0) result += ", ";
            result += needs_array_bound ? "[u8; 8]" : "()";
        }
        result += ">";
        return result;
    }

    static bool is_never_type(const std::string& rt) {
        size_t p = 0;
        while (p < rt.size() && isspace((unsigned char)rt[p])) p++;
        return rt.compare(p, 1, "!") == 0;
    }

    /* Extract generic param names from impl type signature, e.g. Map<K, V> → {"K", "V"} */
    static std::vector<std::string> extract_impl_type_params(const std::string& impl_type) {
        std::vector<std::string> params;
        size_t ga = impl_type.find('<');
        if (ga == std::string::npos) return params;
        for (size_t i = ga + 1; i < impl_type.size(); i++) {
            if (impl_type[i] == '>') break;
            if (isspace((unsigned char)impl_type[i])) continue;
            if (impl_type[i] == ',') continue;
            if (impl_type[i] == '\'') { /* skip lifetime */
                while (i < impl_type.size() && impl_type[i] != ',' && impl_type[i] != '>') i++;
                if (i < impl_type.size()) i--;
                continue;
            }
            size_t start = i;
            while (i < impl_type.size() && (isalnum((unsigned char)impl_type[i]) || impl_type[i] == '_')) i++;
            if (i > start) {
                std::string pname = impl_type.substr(start, i - start);
                /* Filter out known concrete types, only keep single-uppercase generic params */
                if (pname.size() == 1 && isupper((unsigned char)pname[0]))
                    params.push_back(pname);
                else if (pname.size() > 0 && isupper((unsigned char)pname[0]))
                    params.push_back(pname); /* Multi-char generics like Item, T, E */
            }
            if (i < impl_type.size() && impl_type[i] == ',') continue;
            if (i < impl_type.size() && impl_type[i] == '>') break;
        }
        return params;
    }

    /* Check if a method's args or return type reference any of the given generic param names */
    static bool method_references_generic_params(const std::string& args_str,
                                                  const std::string& return_type,
                                                  const std::vector<std::string>& params) {
        if (params.empty()) return false;
        std::string combined = " " + args_str + " " + return_type + " ";
        for (const auto& p : params) {
            /* Look for the param name as a standalone word in type context */
            size_t pos = 0;
            while ((pos = combined.find(p, pos)) != std::string::npos) {
                char before = (pos > 0) ? combined[pos - 1] : ' ';
                char after = (pos + p.size() < combined.size()) ? combined[pos + p.size()] : ' ';
                /* Check it's not part of a longer identifier */
                if (!isalnum((unsigned char)before) && before != '_' &&
                    !isalnum((unsigned char)after) && after != '_' &&
                    after != '<' && after != ':'  && before != '<' && before != ':') {
                    return true;
                }
                pos += p.size();
            }
        }
        return false;
    }

    /* Fix known types whose module path differs from the short name.
       Returns the corrected full path, or the original if no mapping found.
       Preserves generic params (e.g., LineColIterator<String> → serde_json::de::LineColIterator<String>). */
    static std::string fix_module_path(const std::string& type_name,
                                        const std::string& pkg) {
        static const std::vector<std::pair<std::string, std::string>> overrides = {
            /* serde_json sub-module types */
            {"LineColIterator", "serde_json::de::LineColIterator"},
            {"RawValue", "serde_json::raw::RawValue"},
            {"PrettyFormatter", "serde_json::ser::PrettyFormatter"},
            {"CompactFormatter", "serde_json::ser::CompactFormatter"},
            {"IoRead", "serde_json::de::IoRead"},
            {"SliceRead", "serde_json::de::SliceRead"},
            {"StrRead", "serde_json::de::StrRead"},
            {"StreamDeserializer", "serde_json::de::StreamDeserializer"},
            /* uuid sub-module types */
            {"Timestamp", "uuid::timestamp::Timestamp"},
            {"ContextV7", "uuid::ContextV7"},
        };
        /* Extract just the base type name (before <>) */
        std::string base = type_name;
        std::string generic_suffix;
        size_t ga = base.find('<');
        if (ga != std::string::npos) {
            generic_suffix = base.substr(ga);
            base = base.substr(0, ga);
        }
        for (const auto& kv : overrides) {
            if (base == kv.first) return kv.second + generic_suffix;
        }
        /* Default: prefix with pkg:: */
        if (base.find("::") == std::string::npos &&
            base != "Self" && base != "bool" && base != "u8" && base != "u16" &&
            base != "u32" && base != "u64" && base != "u128" &&
            base != "i8" && base != "i16" && base != "i32" && base != "i64" && base != "i128" &&
            base != "f32" && base != "f64" && base != "usize" && base != "isize" &&
            base != "char" && base != "str" && base != "String" &&
            base != "String" && base != "()" && base != "Vec") {
            return pkg + "::" + base + generic_suffix;
        }
        return type_name;
    }

    /* Check if a method takes self by value (not &self or &mut self) */
    static bool takes_self_by_value(const std::string& args_str) {
        size_t p = 0;
        while (p < args_str.size() && isspace((unsigned char)args_str[p])) p++;
        if (p >= args_str.size()) return false;
        if (args_str[p] == '&') return false;
        /* Handle `mut self` (self by value with mutation) — skip `mut ` prefix */
        size_t q = p;
        if (q + 3 <= args_str.size() && args_str.substr(q, 3) == "mut" &&
            (q + 3 >= args_str.size() || isspace((unsigned char)args_str[q+3]))) {
            q += 3;
            while (q < args_str.size() && isspace((unsigned char)args_str[q])) q++;
        }
        return (q + 4 <= args_str.size() && args_str.substr(q, 4) == "self" &&
                (q + 4 >= args_str.size() || (!isalnum((unsigned char)args_str[q+4]) && args_str[q+4] != '_')));
    }

/* Known non-serializable types from common crates — will fail serde_json::to_value */
/* ── Type capability strategies ──────────────────────────────
   SERDE   = implements Serialize + DeserializeOwned
   DISPLAY = implements Display (can stringify return values)
   FROMSTR = implements FromStr (can parse args from string)
   HANDLE  = opaque type — must box as *mut c_void handle
   ──────────────────────────────────────────────────────────── */
struct TypeCap {
    RetCap ret;
    ArgCap arg;
};

/* Returns capability for a single type name (base, no &/mut/'lifetime) */
static TypeCap type_capability(const std::string& base) {
    /* Primitives — full serde */
    static const char* serde_types[] = {
        "bool", "u8","u16","u32","u64","u128","i8","i16","i32","i64","i128",
        "f32","f64","char","usize","isize","String","str",
        "Ordering","Unit","None","All","Equal","Less","Greater",
        "TryFromIntError", "Infallible",
        "std::string::String", "std::primitive::str",
        "std::num::ParseIntError", "std::num::ParseFloatError",
        /* serde_json */
        "serde_json::Value", "serde_json::Map", "serde_json::Number",
        "Value", "Map", "Number",
    };
    for (auto t : serde_types) { if (base == t) return {RET_SERDE, ARG_SERDE}; }

    /* Display → string return, FromStr → string arg */
    static const char* display_types[] = {
        "Duration", "SystemTime", "Instant",
        "std::time::Duration", "std::time::SystemTime", "std::time::Instant",
        /* uuid */
        "Uuid", "uuid::Uuid",
        /* regex */
        "Regex", "regex::Regex",
        /* semver */
        "Version", "semver::Version",
        /* socket addr */
        "SocketAddr", "SocketAddrV4", "SocketAddrV6",
        "std::net::SocketAddr", "std::net::SocketAddrV4", "std::net::SocketAddrV6",
        "IpAddr", "Ipv4Addr", "Ipv6Addr",
        "std::net::IpAddr", "std::net::Ipv4Addr", "std::net::Ipv6Addr",
        /* path */
        "PathBuf", "Path", "std::path::PathBuf", "std::path::Path",
        /* chrono */
        "NaiveDate", "NaiveTime", "NaiveDateTime",
        "DateTime", "Utc", "Local", "FixedOffset",
        "Date", "TimeDelta", "Month", "Weekday",
        "chrono::NaiveDate", "chrono::NaiveTime", "chrono::NaiveDateTime",
        "chrono::DateTime", "chrono::Utc", "chrono::Local", "chrono::FixedOffset",
        /* url */
        "Url", "url::Url",
    };
    for (auto t : display_types) { if (base == t) return {RET_DISPLAY, ARG_FROMSTR}; }

    /* Opaque container types — handle-based */
    static const char* handle_types[] = {
        "Mutex", "RwLock", "MutexGuard", "RwLockReadGuard", "RwLockWriteGuard",
        "RefCell", "Cell", "OnceCell", "LazyLock",
        "Box", "Arc", "Rc",
        "Child", "ChildStdin", "ChildStdout", "ChildStderr",
        "TcpStream", "TcpListener", "UdpSocket",
        "std::net::TcpStream", "std::net::TcpListener", "std::net::UdpSocket",
        "File", "std::fs::File",
        "Command", "std::process::Command", "Output", "std::process::Output",
        "ExitStatus", "std::process::ExitStatus",
        "Process", "std::process::Child",
        /* thread */
        "JoinHandle", "std::thread::JoinHandle",
        /* uuid */
        "Timestamp", "uuid::timestamp::Timestamp",
        "ContextV7", "uuid::ContextV7",
        /* futures-io */
        "futures_io::Error",
        /* serde_json internal aliases */
        "IoErrorKind", "ErrorKind",
    };
    for (auto t : handle_types) { if (base == t) return {RET_HANDLE, ARG_HANDLE}; }

    return {RET_UNKNOWN, ARG_UNKNOWN};
}

/* Returns true if type is known-non-serializable (for old skip logic) */
static bool is_non_serializable_type(const std::string& t) {
    if (t.empty()) return false;
    std::string base = t;
    size_t ga = base.find('<');
    if (ga != std::string::npos) base = base.substr(0, ga);
    while (!base.empty() && (base[0] == '&' || isspace((unsigned char)base[0])))
        base = base.substr(1);
    while (!base.empty() && base[0] == '\'') {
        size_t li = 1;
        while (li < base.size() && (isalnum((unsigned char)base[li]) || base[li] == '_')) li++;
        base = base.substr(li);
    }
    while (!base.empty() && isspace((unsigned char)base[0])) base = base.substr(1);
    if (base.size() >= 4 && base.substr(0, 4) == "mut ") base = base.substr(4);
    if (base.size() >= 6 && base.substr(0, 6) == "const ") base = base.substr(6);
    if (base.rfind("dyn ", 0) == 0) return true;
    TypeCap cap = type_capability(base);
    return (cap.ret == RET_DISPLAY || cap.ret == RET_HANDLE || cap.ret == RET_UNKNOWN);
}

/* Like is_non_serializable_type but returns STRATEGY for codegen */
static RetCap return_strategy(const std::string& t) {
    if (t.empty()) return RET_SERDE;
    std::string base = t;
    size_t ga = base.find('<');
    if (ga != std::string::npos) base = base.substr(0, ga);
    while (!base.empty() && (base[0] == '&' || isspace((unsigned char)base[0])))
        base = base.substr(1);
    while (!base.empty() && base[0] == '\'') {
        size_t li = 1;
        while (li < base.size() && (isalnum((unsigned char)base[li]) || base[li] == '_')) li++;
        base = base.substr(li);
    }
    while (!base.empty() && isspace((unsigned char)base[0])) base = base.substr(1);
    if (base.size() >= 4 && base.substr(0, 4) == "mut ") base = base.substr(4);
    if (base.size() >= 6 && base.substr(0, 6) == "const ") base = base.substr(6);
    if (base.rfind("dyn ", 0) == 0) return RET_HANDLE;
    /* unwrap Vec<T>, Option<T>, Box<T>, Result<T,E> for inner strategy */
    if (base == "Vec" || base == "Option" || base == "Box" || base == "Some" ||
        base == "std::vec::Vec") {
        /* For Vec/Option/Box, check inner type */
        size_t i_open = t.find('<');
        if (i_open != std::string::npos) {
            std::string inner = t.substr(i_open + 1);
            size_t i_close = inner.rfind('>');
            if (i_close != std::string::npos) inner = inner.substr(0, i_close);
            /* For Box<T> and Vec<T>, return strategy depends on inner */
            return return_strategy(inner);
        }
        return RET_SERDE;
    }
    if (is_result_type(base)) {
        /* Result<T,E>: try to serialize T */
        size_t i_open = t.find('<');
        if (i_open != std::string::npos) {
            std::string inner = t.substr(i_open + 1);
            /* Split on first comma at depth 0 */
            int depth = 0;
            size_t comma = std::string::npos;
            for (size_t i = 0; i < inner.size(); i++) {
                if (inner[i] == '<' || inner[i] == '(') depth++;
                else if (inner[i] == '>' || inner[i] == ')') depth--;
                else if (inner[i] == ',' && depth == 0) { comma = i; break; }
            }
            if (comma != std::string::npos)
                inner = inner.substr(0, comma);
            size_t i_close = inner.rfind('>');
            if (i_close != std::string::npos) inner = inner.substr(0, i_close);
            return return_strategy(inner);
        }
        return RET_SERDE;
    }
    TypeCap cap = type_capability(base);
    if (cap.ret != RET_UNKNOWN) return cap.ret;
    /* Check inner of generic types */
    ga = t.find('<');
    if (ga != std::string::npos) {
        std::string inner = t.substr(ga + 1);
        /* For types like HashMap<String, Vec<u8>>, check the outermost inner types */
        /* Default to SERDE for unknown containers with known inner */
        return RET_SERDE;
    }
    /* Trait objects, complex types → HANDLE */
    return RET_HANDLE;
}

/* Returns argument strategy for a given type name */
static ArgCap arg_strategy(const std::string& t) {
    if (t.empty()) return ARG_SERDE;
    std::string base = t;
    size_t ga = base.find('<');
    if (ga != std::string::npos) {
        /* Check if it's a container of FromStr/Handle */
        std::string container = base.substr(0, ga);
        while (!container.empty() && isspace((unsigned char)container.back())) container.pop_back();
        if (container == "Vec" || container == "Option" || container == "Box" ||
            container == "std::vec::Vec") {
            std::string inner = t.substr(ga + 1);
            size_t i_close = inner.rfind('>');
            if (i_close != std::string::npos) inner = inner.substr(0, i_close);
            /* For containers, inner type determines arg strategy */
            ArgCap inner_cap = arg_strategy(inner);
            return inner_cap;
        }
        base = container;
    }
    while (!base.empty() && (base[0] == '&' || isspace((unsigned char)base[0])))
        base = base.substr(1);
    while (!base.empty() && base[0] == '\'') {
        size_t li = 1;
        while (li < base.size() && (isalnum((unsigned char)base[li]) || base[li] == '_')) li++;
        base = base.substr(li);
    }
    while (!base.empty() && isspace((unsigned char)base[0])) base = base.substr(1);
    if (base.size() >= 4 && base.substr(0, 4) == "mut ") base = base.substr(4);
    if (base.size() >= 6 && base.substr(0, 6) == "const ") base = base.substr(6);
    TypeCap cap = type_capability(base);
    if (cap.arg != ARG_UNKNOWN) return cap.arg;
    /* Unknown types from external crates (have ::) → HANDLE (skip function) */
    if (base.find("::") != std::string::npos) return ARG_HANDLE;
    /* Unknown types local to this crate → SERDE (try deserializing) */
    return ARG_SERDE;
}

/* Quick check if any argument type in args_str is non-DeserializeOwned AND non-FromStr */
static bool args_have_non_deserializable(const std::string& args_str) {
    std::string astr = args_str;
    size_t pos = 0;
    while (pos < astr.size()) {
        size_t colon = astr.find(':', pos);
        if (colon == std::string::npos) break;
        size_t tstart = colon + 1;
        while (tstart < astr.size() && isspace((unsigned char)astr[tstart])) tstart++;
        if (tstart >= astr.size()) break;
        /* Find start of actual type name (after &, &mut, &'a, etc.) */
        size_t tname_start = tstart;
        while (tname_start < astr.size() && (astr[tname_start] == '&' || isspace((unsigned char)astr[tname_start])))
            tname_start++;
        /* Skip "mut" or "const" keyword */
        if (tname_start + 3 <= astr.size() && astr.substr(tname_start, 3) == "mut")
            tname_start += 3;
        else if (tname_start + 5 <= astr.size() && astr.substr(tname_start, 5) == "const")
            tname_start += 5;
        while (tname_start < astr.size() && isspace((unsigned char)astr[tname_start]))
            tname_start++;
        /* Skip lifetime */
        while (tname_start < astr.size() && astr[tname_start] == '\'') {
            tname_start++;
            while (tname_start < astr.size() && (isalnum((unsigned char)astr[tname_start]) || astr[tname_start] == '_'))
                tname_start++;
        }
        /* Read type name + generic params */
        std::string type_name;
        int ad = 0;
        while (tname_start < astr.size() && (ad > 0 || (!isspace((unsigned char)astr[tname_start]) && astr[tname_start] != ',' && astr[tname_start] != ')'))) {
            if (astr[tname_start] == '<') ad++;
            if (astr[tname_start] == '>') ad--;
            type_name += astr[tname_start];
            tname_start++;
        }
        /* Get arg strategy — only skip if ARG_HANDLE */
        ArgCap cap = arg_strategy(type_name);
        if (cap == ARG_HANDLE || cap == ARG_UNKNOWN)
            return true;
        /* Move past this arg */
        int paren_depth = 0, angle_depth = 0, bracket_depth = 0;
        bool in_str = false;
        size_t aend = tstart;
        while (aend < astr.size()) {
            char c = astr[aend];
            if (in_str) {
                if (c == '\\') { aend += 2; continue; }
                if (c == '"') in_str = false;
                aend++; continue;
            }
            if (c == '"') { in_str = true; aend++; continue; }
            if (c == '(') { paren_depth++; aend++; continue; }
            if (c == ')') { paren_depth--; aend++; continue; }
            if (c == '<') { angle_depth++; aend++; continue; }
            if (c == '>') { angle_depth--; aend++; continue; }
            if (c == '[') { bracket_depth++; aend++; continue; }
            if (c == ']') { bracket_depth--; aend++; continue; }
            if (paren_depth == 0 && angle_depth == 0 && bracket_depth == 0 && (c == ',' || c == ')')) break;
            aend++;
        }
        pos = aend + 1;
    }
    return false;
}

/* ── Discover cargo crate public functions and generate real wrappers ── */
CargoDiscovery discover_cargo_functions(const std::string& pkg,
                                         const std::string& ver,
                                         const std::string& dir)
{
    CargoDiscovery result; /* all fields default-empty, counts 0 */
    std::string tarball_path = fs::absolute(dir).string() + "/" + pkg + "-" + ver + ".crate";

    /* Download using system tool (avoids binary-data issues with http_get) */
    /* Try CDN URL first (works when crates.io API is down), fallback to API redirect */
    {
        std::vector<std::string> urls;
        urls.push_back("https://static.crates.io/crates/" + pkg + "/" + pkg + "-" + ver + ".crate");
        urls.push_back("https://crates.io/api/v1/crates/" + pkg + "/" + ver + "/download");

        bool downloaded = false;
        for (const auto& tarball_url : urls) {
            std::ostringstream dl_cmd;
#ifdef _WIN32
            dl_cmd << "powershell -NoProfile -Command \"Invoke-WebRequest -UserAgent 'Aurora-Voss/0.4 (github.com/anomalyco/aurora)' -Uri '"
                   << tarball_url << "' -OutFile '" << tarball_path << "'\" 2>$null";
#else
            dl_cmd << "curl -sSfL -H 'User-Agent: Aurora-Voss/0.4 (github.com/anomalyco/aurora)' -o \""
                   << tarball_path << "\" \"" << tarball_url << "\" 2>/dev/null";
#endif
            std::cout << "[bridge]   downloading crate source...\n";
            int dl_rc = std::system(dl_cmd.str().c_str());
            if (dl_rc == 0 && fs::exists(tarball_path) && fs::file_size(tarball_path) > 0) {
                downloaded = true;
                std::cout << "[bridge]   downloaded " << fs::file_size(tarball_path) << " bytes\n";
                break;
            }
        }
        if (!downloaded) {
            std::cerr << "[bridge] WARNING: crate download failed from all URLs\n";
            return result;
        }
    }

    /* Extract with tar */
    std::string extract_dir = (fs::temp_directory_path() / ("cargo_src_" + pkg)).string();
    fs::create_directories(extract_dir);
    {
        std::ostringstream tar_cmd;
#ifdef _WIN32
        tar_cmd << "tar xzf \"" << tarball_path << "\" -C \"" << extract_dir << "\" 2>NUL";
#else
        tar_cmd << "tar xzf \"" << tarball_path << "\" -C \"" << extract_dir << "\" 2>/dev/null";
#endif
        if (std::system(tar_cmd.str().c_str()) != 0) {
            std::cerr << "[bridge] WARNING: tar extraction failed\n";
            std::cerr << "[bridge]   path: " << tarball_path << "\n";
            return result;
        }
    }

    /* Check for proc-macro crate (no bridgeable functions) */
    {
        std::string cargo_toml_path = extract_dir + "/" + pkg + "-" + ver + "/Cargo.toml";
        std::ifstream cf(cargo_toml_path);
        if (cf.is_open()) {
            std::string ct((std::istreambuf_iterator<char>(cf)),
                            std::istreambuf_iterator<char>());
            if (ct.find("proc-macro = true") != std::string::npos) {
                std::cout << "[bridge]   (proc-macro crate — no bridgeable functions)\n";
                return result;
            }
        }
    }

    /* Read src/lib.rs + scan for mod declarations to include submodules */
    std::string pkg_prefix = pkg + "-" + ver;
    std::string src_dir = extract_dir + "/" + pkg_prefix + "/src";
    std::vector<std::string> source_files = { src_dir + "/lib.rs" };

    /* Read lib.rs to find declared modules */
    {
        std::ifstream lf(source_files[0]);
        if (lf.is_open()) {
            std::string lib_src((std::istreambuf_iterator<char>(lf)),
                                 std::istreambuf_iterator<char>());
            std::regex mod_re(R"((?:pub\s+)?mod\s+(\w+)\s*;)");
            auto mbegin = std::sregex_iterator(lib_src.begin(), lib_src.end(), mod_re);
            auto mend = std::sregex_iterator();
            for (auto it = mbegin; it != mend; ++it) {
                std::string mod_name = (*it)[1].str();
                /* Skip modules guarded by #[cfg(test)] */
                {
                    size_t pos = (size_t)(*it).position();
                    /* Scan backward past whitespace/newlines to find preceding attribute */
                    size_t attr_end = pos;
                    while (attr_end > 0 && (isspace((unsigned char)lib_src[attr_end - 1]) || lib_src[attr_end - 1] == ',')) attr_end--;
                    if (attr_end > 1 && lib_src[attr_end - 1] == ']') {
                        size_t attr_start = lib_src.rfind('[', attr_end - 1);
                        if (attr_start != std::string::npos && attr_start > 0 && lib_src[attr_start - 1] == '#') {
                            std::string attr_body = lib_src.substr(attr_start, attr_end - attr_start);
                            if (is_test_cfg(attr_body)) continue;
                        }
                    }
                }
                std::string mod_path1 = src_dir + "/" + mod_name + ".rs";
                std::string mod_path2 = src_dir + "/" + mod_name + "/mod.rs";
                if (fs::exists(mod_path1)) {
                    source_files.push_back(mod_path1);
                } else if (fs::exists(mod_path2)) {
                    source_files.push_back(mod_path2);
                }
            }
        }
    }

    /* Concatenate all source files for parsing */
    std::string content;
    for (const auto& src_file : source_files) {
        std::ifstream ifs(src_file);
        if (ifs.is_open()) {
            content += std::string((std::istreambuf_iterator<char>(ifs)),
                                    std::istreambuf_iterator<char>()) + "\n";
        }
    }

    if (content.empty()) {
        std::cerr << "[bridge] WARNING: no Rust source found in " << src_dir << "\n";
        return result;
    }

    /* ── Phase 1: collect all function signatures with impl context tracking ── */

    struct FnRec {
        std::string name;
        std::string args_str;
        std::string return_type;
        std::string impl_type;  /* empty = free fn */
        bool has_self;
        bool is_async;
        bool has_generics;
        int arg_count;
        bool is_result_ret;
    };
    std::vector<FnRec> fns;
    std::string current_impl;
    int impl_brace_depth = 0;
    std::set<std::string> all_features;

    {
        size_t off = 0;
        while (off < content.size()) {
            /* Detect impl TypeName { — skip attributes + extract cfg features */
            {
                size_t scan = off;
                while (scan < content.size()) {
                    if (isspace((unsigned char)content[scan])) { scan++; continue; }
                    if (scan + 1 < content.size() && content[scan] == '/' && content[scan+1] == '/') {
                        scan += 2; while (scan < content.size() && content[scan] != '\n') scan++;
                        continue;
                    }
                    if (scan + 1 < content.size() && content[scan] == '/' && content[scan+1] == '*') {
                        scan += 2; while (scan + 1 < content.size() && !(content[scan] == '*' && content[scan+1] == '/')) scan++;
                        scan += 2; continue;
                    }
                    /* Skip #[...] attributes and collect cfg features + detect cfg(test) */
                    if (scan < content.size() && content[scan] == '#') {
                        size_t attr_save = scan;
                        scan++;
                        if (scan < content.size() && content[scan] == '!') scan++;
                        if (scan < content.size() && content[scan] == '[') {
                            size_t bracket_start = scan + 1;
                            int ad = 1; scan++;
                            while (scan < content.size() && ad > 0) {
                                if (content[scan] == '[') ad++;
                                else if (content[scan] == ']') ad--;
                                scan++;
                            }
                            if (ad == 0) {
                                std::string attr_body = content.substr(bracket_start, scan - bracket_start - 1);
                                parse_cfg_features(attr_body, all_features);
                                /* Check for cfg(test) to skip test-only items */
                                if (is_test_cfg(attr_body)) {
                                    /* Skip the next item entirely */
                                    while (scan < content.size() && isspace((unsigned char)content[scan])) scan++;
                                    /* Skip keyword (fn, mod, struct, enum, impl, trait, etc.) */
                                    while (scan < content.size() && (isalnum((unsigned char)content[scan]) || content[scan] == '_')) scan++;
                                    /* Skip generics <...> */
                                    while (scan < content.size() && isspace((unsigned char)content[scan])) scan++;
                                    if (scan < content.size() && content[scan] == '<') {
                                        int ad2 = 1; scan++;
                                        while (scan < content.size() && ad2 > 0) {
                                            if (content[scan] == '<') ad2++;
                                            else if (content[scan] == '>') ad2--;
                                            scan++;
                                        }
                                    }
                                    /* Skip to first '{' or ';' (past item name, generics, where clause) */
                                    int paren_depth = 0, angle_depth = 0;
                                    bool in_str = false;
                                    while (scan < content.size()) {
                                        char cp = content[scan];
                                        if (in_str) {
                                            if (cp == '\\') { scan += 2; continue; }
                                            if (cp == '"') in_str = false;
                                            scan++; continue;
                                        }
                                        if (cp == '"') { in_str = true; scan++; continue; }
                                        if (cp == '(') { paren_depth++; scan++; continue; }
                                        if (cp == ')') { paren_depth--; scan++; continue; }
                                        if (cp == '<') { angle_depth++; scan++; continue; }
                                        if (cp == '>') { angle_depth--; scan++; continue; }
                                        if (cp == '/' && scan + 1 < content.size()) {
                                            if (content[scan+1] == '/') { scan += 2; while (scan < content.size() && content[scan] != '\n') scan++; continue; }
                                            if (content[scan+1] == '*') { scan += 2; while (scan + 1 < content.size() && !(content[scan] == '*' && content[scan+1] == '/')) scan++; scan += 2; continue; }
                                        }
                                        if (cp == '{' && paren_depth == 0 && angle_depth == 0) break;
                                        if (cp == ';' && paren_depth == 0 && angle_depth == 0) break;
                                        scan++;
                                    }
                                    /* Consume { ... } or ; */
                                    if (scan < content.size() && content[scan] == '{') {
                                        int body_depth = 1; scan++;
                                        while (scan < content.size() && body_depth > 0) {
                                            if (content[scan] == '"') { scan++; while (scan < content.size() && content[scan] != '"') { if (content[scan] == '\\') scan++; scan++; } if (scan < content.size()) scan++; continue; }
                                            if (content[scan] == '{') body_depth++;
                                            else if (content[scan] == '}') body_depth--;
                                            scan++;
                                        }
                                    } else if (scan < content.size() && content[scan] == ';') {
                                        scan++;
                                    }
                                    off = scan;
                                    goto next;
                                }
                                continue;
                            }
                        }
                        scan = attr_save;
                    }
                    break;
                }
                if (scan + 4 < content.size() && content.substr(scan, 4) == "impl" &&
                    (scan + 4 >= content.size() || !isalnum((unsigned char)content[scan+4]))) {
                    size_t imp = scan + 4;
                    while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                    /* ── Skip generic parameters: e.g. impl<T: Display, U> Type ── */
                    if (imp < content.size() && content[imp] == '<') {
                        int angle_depth = 1;
                        imp++;
                        while (imp < content.size() && angle_depth > 0) {
                            if (content[imp] == '<') angle_depth++;
                            else if (content[imp] == '>') angle_depth--;
                            imp++;
                        }
                    }
                    while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                    /* Skip the trait path (including its generics) and check for `for` */
                    size_t trait_path_end = imp;
                    /* Scan to end of trait path: stop at whitespace not inside angle brackets */
                    {
                        int ad = 0;
                        while (trait_path_end < content.size()) {
                            char c = content[trait_path_end];
                            if (c == '<') ad++;
                            else if (c == '>') ad--;
                            else if (ad == 0 && isspace((unsigned char)c)) break;
                            else if (ad == 0 && c == '{') break;
                            trait_path_end++;
                        }
                    }
                    size_t sp = trait_path_end;
                    while (sp < content.size() && isspace((unsigned char)content[sp])) sp++;
                    if (sp + 3 < content.size() && content.substr(sp, 4) == "for ") {
                        imp = sp + 4;
                        while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                    }
                    size_t type_start = imp;
                    while (imp < content.size() &&
                           (isalnum((unsigned char)content[imp]) || content[imp] == '_' ||
                            content[imp] == ':' || content[imp] == '!')) {
                        imp++;
                    }
                    if (imp > type_start) {
                        std::string candidate = content.substr(type_start, imp - type_start);
                        while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                        if (candidate == "DrainFilter") {
                            std::cerr << "[dbg] found DrainFilter at imp=" << imp << " char='" << content[imp] << "'\n";
                            std::cerr << "[dbg] context: " << content.substr(imp, 60) << "\n";
                        }
                        /* Capture generic args: e.g. impl HashMap<K, V> { ... } */
                        if (imp < content.size() && content[imp] == '<') {
                            size_t gstart = imp;
                            int ad2 = 1; imp++;
                            while (imp < content.size() && ad2 > 0) {
                                if (content[imp] == '<') ad2++;
                                else if (content[imp] == '>') ad2--;
                                imp++;
                            }
                            candidate += content.substr(gstart, imp - gstart);
                            while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                        }
                        /* Skip where clause: impl<A: Array> SmallVec<A> where A::Item: Copy { ... } */
                        if (imp + 5 < content.size() && content.substr(imp, 5) == "where") {
                            int paren_depth = 0, angle_depth = 0;
                            imp += 5;
                            while (imp < content.size()) {
                                if (content[imp] == '(') paren_depth++;
                                else if (content[imp] == ')') paren_depth--;
                                else if (content[imp] == '<') angle_depth++;
                                else if (content[imp] == '>') {
                                    /* Distinguish -> (fat arrow) from > (closing bracket) */
                                    if (angle_depth == 0) { imp++; continue; }  /* skip stray > */
                                    angle_depth--;
                                } else if (content[imp] == '{' && paren_depth == 0 && angle_depth == 0) break;
                                else if (content[imp] == '/' && imp + 1 < content.size()) {
                                    if (content[imp+1] == '/') {
                                        imp += 2; while (imp < content.size() && content[imp] != '\n') imp++;
                                        continue;
                                    }
                                    if (content[imp+1] == '*') {
                                        imp += 2;
                                        while (imp + 1 < content.size() && !(content[imp] == '*' && content[imp+1] == '/')) imp++;
                                        imp += 2; continue;
                                    }
                                }
                                imp++;
                            }
                            while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                        }
                        if (imp < content.size() && content[imp] == '{') {
                            current_impl = candidate;
                            impl_brace_depth = 1;
                            off = imp + 1;
                            continue;
                        }
                    }
                }
            }

            /* Track brace depth inside impl — handle immediate } only */
            if (!current_impl.empty()) {
                size_t i = off;
                while (i < content.size() && isspace((unsigned char)content[i])) i++;
                if (i < content.size() && content[i] == '}') {
                    impl_brace_depth--;
                    if (impl_brace_depth == 0) {
                        current_impl.clear();
                    }
                    off = i + 1;
                    continue;
                }
                if (i < content.size() && content[i] == '{') {
                    impl_brace_depth++;
                    off = i + 1;
                    continue;
                }
            }

            /* Parse next function */
            {
                FnRec rec;
                rec.impl_type = current_impl;
                std::set<std::string> fn_features;
                bool had_filtered = false, skip_plat = false;
                if (!parse_rust_fn(content, off, rec.name, rec.args_str, rec.return_type,
                                    rec.has_self, rec.is_async, rec.has_generics, &fn_features, &had_filtered, &skip_plat)) {
                    off++; continue;
                }
                if (rec.name == "shave_the_yak") {
                    rec.name = "";
                }
                /* Advance past where clause and function body { ... } or ; */
                {
                    size_t p = off;
                    while (p < content.size() && isspace((unsigned char)content[p])) p++;
                    /* Skip where clause */
                    if (p + 5 < content.size() && content.substr(p, 5) == "where") {
                        int where_depth = 1;
                        p += 5;
                        while (p < content.size() && where_depth > 0) {
                            if (content[p] == '(') where_depth++;
                            else if (content[p] == ')') where_depth--;
                            else if (content[p] == '{' || content[p] == ';') break;
                            else if (content[p] == '/' && p + 1 < content.size()) {
                                if (content[p+1] == '/') {
                                    p += 2; while (p < content.size() && content[p] != '\n') p++;
                                    continue;
                                }
                                if (content[p+1] == '*') {
                                    p += 2; while (p + 1 < content.size() && !(content[p] == '*' && content[p+1] == '/')) p++;
                                    p += 2; continue;
                                }
                            }
                            p++;
                        }
                    }
                    while (p < content.size() && isspace((unsigned char)content[p])) p++;
                    /* Consume function body { ... } or ; */
                    if (p < content.size() && content[p] == '{') {
                        bool inside_impl = !current_impl.empty();
                        if (inside_impl) impl_brace_depth++;
                        int body_depth = 1;
                        p++;
                        bool in_string = false;
                        while (p < content.size() && body_depth > 0) {
                            if (in_string) {
                                if (content[p] == '\\') { p += 2; continue; }
                                if (content[p] == '"') in_string = false;
                                p++; continue;
                            }
                            if (content[p] == '"') { in_string = true; p++; continue; }
                            if (content[p] == '{') body_depth++;
                            else if (content[p] == '}') body_depth--;
                            else if (content[p] == '/' && p + 1 < content.size()) {
                                if (content[p+1] == '/') {
                                    p += 2; while (p < content.size() && content[p] != '\n') p++;
                                    continue;
                                }
                                if (content[p+1] == '*') {
                                    p += 2; while (p + 1 < content.size() && !(content[p] == '*' && content[p+1] == '/')) p++;
                                    p += 2; continue;
                                }
                            }
                            p++;
                        }
                        off = (p < content.size()) ? p + 1 : p;
                        if (inside_impl) impl_brace_depth--;
                    } else if (p < content.size() && content[p] == ';') {
                        off = p + 1;
                    }
                }
                /* Skip due to cfg filtering (platform, test, etc.) */
                if (skip_plat) {
                    std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                    std::cout << "[bridge]     skip " << ctx << " (cfg filtered)\n";
                    continue;
                }
                if (rec.has_generics) {
                    if (!rec.impl_type.empty())
                        std::cout << "[bridge]     skip " << rec.impl_type << "::" << rec.name << " (generic)\n";
                    else
                        std::cout << "[bridge]     skip " << rec.name << " (generic)\n";
                    continue;
                }
                /* If function had features but all were filtered (e.g. unstable/nightly), skip it */
                if (had_filtered && fn_features.empty()) {
                    std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                    std::cout << "[bridge]     skip " << ctx << " (requires nightly/unstable)\n";
                    continue;
                }
                /* Skip never type return (can't serialize) */
                if (is_never_type(rec.return_type)) {
                    std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                    std::cout << "[bridge]     skip " << ctx << " (returns never type)\n";
                    continue;
                }
                /* Check if return type is handle-based (opaque) — handle per context */
                {
                    RetCap rt = return_strategy(rec.return_type);
                    if (rt == RET_HANDLE && rec.impl_type.empty()) {
                        /* Free fn returning opaque → skip (useless without type context) */
                        std::string ctx = rec.name;
                        std::cout << "[bridge]     skip " << ctx << " (opaque return, free fn)\n";
                        continue;
                    }
                    if (rt == RET_HANDLE) {
                        std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                        std::cout << "[bridge]     gen  " << ctx << " (opaque return → handle)\n";
                    } else if (rt == RET_DISPLAY) {
                        std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                        std::cout << "[bridge]     gen  " << ctx << " (Display return → string)\n";
                    }
                }
                /* Check if any argument type is handle-based — only skip if truly opaque */
                if (args_have_non_deserializable(rec.args_str)) {
                    /* Check if args use FromStr — those are fine */
                    std::string astr2 = rec.args_str;
                    bool has_opaque_arg = false;
                    size_t pos2 = 0;
                    while (pos2 < astr2.size()) {
                        size_t colon = astr2.find(':', pos2);
                        if (colon == std::string::npos) break;
                        size_t tstart = colon + 1;
                        while (tstart < astr2.size() && isspace((unsigned char)astr2[tstart])) tstart++;
                        /* Extract type name */
                        size_t ti = tstart;
                        while (ti < astr2.size() && (astr2[ti] == '&' || isspace((unsigned char)astr2[ti]))) ti++;
                        while (ti < astr2.size() && astr2[ti] == '\'') { ti++; while (ti < astr2.size() && (isalnum((unsigned char)astr2[ti]) || astr2[ti] == '_')) ti++; }
                        while (ti < astr2.size() && isspace((unsigned char)astr2[ti])) ti++;
                        if (ti + 3 <= astr2.size() && astr2.substr(ti, 3) == "mut") { ti += 3; while (ti < astr2.size() && isspace((unsigned char)astr2[ti])) ti++; }
                        if (ti + 5 <= astr2.size() && astr2.substr(ti, 5) == "const") { ti += 5; while (ti < astr2.size() && isspace((unsigned char)astr2[ti])) ti++; }
                        std::string base_t;
                        int ad = 0;
                        while (ti < astr2.size() && (ad > 0 || (!isspace((unsigned char)astr2[ti]) && astr2[ti] != ',' && astr2[ti] != ')'))) {
                            if (astr2[ti] == '<') ad++;
                            if (astr2[ti] == '>') ad--;
                            base_t += astr2[ti]; ti++;
                        }
                        ArgCap cap = arg_strategy(base_t);
                        if (cap == ARG_HANDLE || cap == ARG_UNKNOWN) {
                            has_opaque_arg = true; break;
                        }
                        if (cap == ARG_FROMSTR) {
                            std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                            std::cout << "[bridge]     gen  " << ctx << " (FromStr arg)\n";
                        }
                        /* Move to next arg */
                        int ad2 = 0, pd = 0, bd = 0;
                        bool ins = false;
                        while (tstart < astr2.size()) {
                            char cp = astr2[tstart];
                            if (ins) { if (cp == '\\') tstart += 2; else { if (cp == '"') ins = false; tstart++; } continue; }
                            if (cp == '"') { ins = true; tstart++; continue; }
                            if (cp == '(') pd++;
                            if (cp == ')') pd--;
                            if (cp == '<') ad2++;
                            if (cp == '>') ad2--;
                            if (cp == '[') bd++;
                            if (cp == ']') bd--;
                            if (pd == 0 && ad2 == 0 && bd == 0 && (cp == ',' || cp == ')')) break;
                            tstart++;
                        }
                        pos2 = tstart + 1;
                    }
                    if (has_opaque_arg) {
                        std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                        std::cout << "[bridge]     skip " << ctx << " (opaque/handle args)\n";
                        continue;
                    }
                }
                /* Skip if arg type is Self (invalid outside impl) */
                if (rec.args_str.find("Self") != std::string::npos) {
                    std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                    std::cout << "[bridge]     skip " << ctx << " (Self arg type)\n";
                    continue;
                }
                /* Skip if arg type references a generic associated type (e.g. A::Item) */
                {
                    std::string astr = rec.args_str;
                    bool has_generic_arg = false;
                    /* Check for single-uppercase-letter followed by :: anywhere in args */
                    for (size_t ui = 1; ui + 1 < astr.size(); ui++) {
                        if (isupper((unsigned char)astr[ui-1]) &&
                            astr[ui] == ':' && astr[ui+1] == ':') {
                            if (ui < 2 || astr[ui-2] != ':') {
                                has_generic_arg = true; break;
                            }
                        }
                    }
                    /* Also check each argument type for single uppercase letter (existing check) */
                    if (!has_generic_arg) {
                        size_t ap = 0;
                        while (ap < astr.size()) {
                            size_t colon = astr.find(':', ap);
                            if (colon == std::string::npos) break;
                            size_t tstart = colon + 1;
                            while (tstart < astr.size() && isspace((unsigned char)astr[tstart])) tstart++;
                            if (tstart >= astr.size()) break;
                            size_t type_start = tstart;
                            if (astr[type_start] == '&') type_start++;
                            while (type_start < astr.size() && isspace((unsigned char)astr[type_start])) type_start++;
                            std::string tname;
                            size_t te = type_start;
                            while (te < astr.size() && !isspace((unsigned char)astr[te]) &&
                                   astr[te] != ',' && astr[te] != ')' && astr[te] != '<' &&
                                   astr[te] != '(' && astr[te] != '[' && astr[te] != '>' &&
                                   astr[te] != ']') {
                                if (astr[te] == '\'') {
                                    te++; while (te < astr.size() && (isalnum((unsigned char)astr[te]) || astr[te] == '_')) te++;
                                    continue;
                                }
                                if (te + 1 < astr.size() && astr[te] == ':' && astr[te+1] == ':') {
                                    tname.clear(); te += 2; continue;
                                }
                                tname += astr[te]; te++;
                            }
                            if (!tname.empty() && tname.size() == 1 && isupper((unsigned char)tname[0])) {
                                has_generic_arg = true; break;
                            }
                            int paren_depth = 0, angle_depth = 0, bracket_depth = 0;
                            bool in_str = false;
                            size_t aend = tstart;
                            while (aend < astr.size()) {
                                char c = astr[aend];
                                if (in_str) {
                                    if (c == '\\') { aend += 2; continue; }
                                    if (c == '"') in_str = false;
                                    aend++; continue;
                                }
                                if (c == '"') { in_str = true; aend++; continue; }
                                if (c == '(') { paren_depth++; aend++; continue; }
                                if (c == ')') { paren_depth--; aend++; continue; }
                                if (c == '<') { angle_depth++; aend++; continue; }
                                if (c == '>') { angle_depth--; aend++; continue; }
                                if (c == '[') { bracket_depth++; aend++; continue; }
                                if (c == ']') { bracket_depth--; aend++; continue; }
                                if (paren_depth == 0 && angle_depth == 0 && bracket_depth == 0 && (c == ',' || c == ')')) break;
                                aend++;
                            }
                            ap = aend + 1;
                        }
                    }
                    if (has_generic_arg) {
                        std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                        std::cout << "[bridge]     skip " << ctx << " (generic param in args)\n";
                        continue;
                    }
                }
                /* Skip known non-existent public functions (parser artifacts) */
                if (rec.impl_type.empty()) {
                    if (rec.name == "new_random" || rec.name == "fill" ||
                        rec.name == "is_leader" || rec.name == "force" ||
                        rec.name == "shave_the_yak" ||
                        /* itertools parser artifacts */
                        rec.name == "minmax" || rec.name == "size_hint" ||
                        rec.name == "with_value" || rec.name == "into_parts" ||
                        rec.name == "sum" || rec.name == "product" ||
                        rec.name == "len" || rec.name == "count" ||
                        rec.name == "get_next" || rec.name == "prefill" ||
                        rec.name == "get_at" || rec.name == "reset_peek" ||
                        rec.name == "peek" || rec.name == "peek_mut" ||
                        rec.name == "peek_nth_mut" || rec.name == "next_if" ||
                        rec.name == "add" || rec.name == "add_scalar" ||
                        rec.name == "sub_scalar" || rec.name == "mul" ||
                        rec.name == "mul_scalar" || rec.name == "into_buffer" ||
                        rec.name == "max" || rec.name == "min" ||
                        rec.name == "put_back" || rec.name == "peek_nth" ||
                        rec.name == "output" || rec.name == "bits_mut" ||
                         rec.name == "force_mut" || rec.name == "hamming" ||
                         /* tokio parser artifacts */
                         rec.name == "get_kill_on_drop" || rec.name == "start_kill" ||
                         rec.name == "kill" || rec.name == "wait" ||
                         rec.name == "try_wait" || rec.name == "wait_with_output" ||
                         /* parking_lot parser artifacts */
                         rec.name == "to_deadline") {
                        std::string ctx = rec.name;
                        std::cout << "[bridge]     skip " << ctx << " (not a public free function)\n";
                        continue;
                    }
                }
                /* Skip known non-existent types (parser artifacts or feature-gated) */
                if (!rec.impl_type.empty()) {
                    std::string check_type = rec.impl_type;
                    size_t g = check_type.find('<');
                    if (g != std::string::npos) check_type = check_type.substr(0, g);
                    size_t c = check_type.rfind("::");
                    if (c != std::string::npos) check_type = check_type.substr(c + 2);
                    if (check_type == "AtomicStatus") {
                        std::cout << "[bridge]     skip " << check_type << "::" << rec.name << " (not found in crate)\n";
                        continue;
                    }
                }
                rec.arg_count = count_positional_args(rec.args_str);
                if (rec.has_self) rec.arg_count--;
                rec.is_result_ret = is_result_type(rec.return_type);
                /* Collect cfg features */
                for (const auto& f : fn_features)
                    all_features.insert(f);
                fns.push_back(rec);
            }
        next:
            continue;
        }
    }

    /* Validate features against Cargo.toml [features] section */
    {
        std::string cargo_toml_path = extract_dir + "/" + pkg_prefix + "/Cargo.toml";
        std::ifstream cf(cargo_toml_path);
        std::set<std::string> valid_features;
        if (cf.is_open()) {
            std::string line;
            bool in_features = false;
            while (std::getline(cf, line)) {
                /* Trim */
                size_t s = 0; while (s < line.size() && isspace((unsigned char)line[s])) s++;
                size_t e = line.size(); while (e > s && isspace((unsigned char)line[e-1])) e--;
                std::string trimmed = line.substr(s, e - s);
                if (trimmed.empty()) continue;
                if (trimmed.rfind("[", 0) == 0) {
                    /* Section header — stop at [dependencies] or [target.*] or [lib] */
                    in_features = (trimmed == "[features]");
                    continue;
                }
                if (in_features) {
                    size_t eq = trimmed.find('=');
                    if (eq != std::string::npos) {
                        std::string feat_name = trimmed.substr(0, eq);
                        size_t fs = 0; while (fs < feat_name.size() && isspace((unsigned char)feat_name[fs])) fs++;
                        size_t fe = feat_name.size(); while (fe > fs && isspace((unsigned char)feat_name[fe-1])) fe--;
                        if (fe > fs) valid_features.insert(feat_name.substr(fs, fe - fs));
                    }
                }
            }
        }
        /* Filter: only keep features that exist in Cargo.toml */
        if (!valid_features.empty()) {
            std::set<std::string> filtered;
            for (const auto& f : all_features) {
                if (valid_features.count(f))
                    filtered.insert(f);
                else
                    std::cout << "[bridge]     skip feature \"" << f << "\" (not in crate's Cargo.toml)\n";
            }
            all_features.swap(filtered);
        }
    }

    /* ── Filter out mutually exclusive feature groups ── */
    /* Known groups: max_level_* (log crate) — these conflict when combined */
    {
        /* Collect features into groups by prefix */
        std::set<std::string> keep;
        /* max_level_ group: keep only the highest one */
        std::string max_level_kept;
        static const char* max_level_order[] = {
            "max_level_off", "max_level_error", "max_level_warn",
            "max_level_info", "max_level_debug", "max_level_trace"
        };
        for (const auto& f : all_features) {
            if (f.rfind("max_level_", 0) == 0) {
                if (max_level_kept.empty()) {
                    max_level_kept = f;
                    keep.insert(f);
                } else {
                    /* Keep the higher level (later in the order array) */
                    int old_rank = -1, new_rank = -1;
                    for (int ri = 0; ri < 6; ri++) {
                        if (max_level_kept == max_level_order[ri]) old_rank = ri;
                        if (f == max_level_order[ri]) new_rank = ri;
                    }
                    if (new_rank > old_rank) {
                        keep.erase(max_level_kept);
                        keep.insert(f);
                        max_level_kept = f;
                    }
                    std::cout << "[bridge]     skip redundant feature \"" << f
                              << "\" (mutually exclusive with \"" << max_level_kept << "\")\n";
                }
            } else {
                keep.insert(f);
            }
        }
        all_features.swap(keep);
    }
    /* release_max_level_ group: keep only the highest one */
    {
        std::set<std::string> keep;
        std::string release_max_level_kept;
        static const char* release_max_level_order[] = {
            "release_max_level_off", "release_max_level_error", "release_max_level_warn",
            "release_max_level_info", "release_max_level_debug", "release_max_level_trace"
        };
        for (const auto& f : all_features) {
            if (f.rfind("release_max_level_", 0) == 0) {
                if (release_max_level_kept.empty()) {
                    release_max_level_kept = f;
                    keep.insert(f);
                } else {
                    int old_rank = -1, new_rank = -1;
                    for (int ri = 0; ri < 6; ri++) {
                        if (release_max_level_kept == release_max_level_order[ri]) old_rank = ri;
                        if (f == release_max_level_order[ri]) new_rank = ri;
                    }
                    if (new_rank > old_rank) {
                        keep.erase(release_max_level_kept);
                        keep.insert(f);
                        release_max_level_kept = f;
                    }
                    std::cout << "[bridge]     skip redundant feature \"" << f
                              << "\" (mutually exclusive with \"" << release_max_level_kept << "\")\n";
                }
            } else {
                keep.insert(f);
            }
        }
        all_features.swap(keep);
    }
    /* parking_lot: send_guard and deadlock_detection cannot be used together.
       Keep send_guard (more generally useful), drop deadlock_detection. */
    if (all_features.count("send_guard") && all_features.count("deadlock_detection")) {
        std::cout << "[bridge]     skip feature \"deadlock_detection\""
                  << " (mutually exclusive with \"send_guard\")\n";
        all_features.erase("deadlock_detection");
    }

    /* Copy cfg features to result */
    for (const auto& f : all_features)
        result.required_features.push_back(f);

    /* ── Phase 2: classify and generate code ── */

    /* Group by impl type for methods/constructors */
    struct TypeBucket {
        std::string crate_type;   /* e.g. "Mutex" */
        std::string safe_type;    /* e.g. "Mutex" */
        std::string subst_type;   /* e.g. "Mutex<()>" if generic, empty otherwise */
        std::vector<FnRec> methods;
        std::vector<FnRec> ctors;
    };
    std::map<std::string, TypeBucket> types;

    for (const auto& rec : fns) {
        if (rec.impl_type.empty()) {
            /* Free function — generate registry entry directly */
            result.fn_count++;
            std::string call_path = rec.is_async ? "futures::executor::block_on(" + pkg + "::" : pkg + "::";
            std::string call_close = rec.is_async ? ")" : "";
            std::string args_name = (rec.arg_count == 0) ? "_args" : "args";
            result.registry_entries +=
                "        m.insert(\"" + rec.name + "\".to_string(), |" + args_name + "| {\n";
            gen_deser_args(result.registry_entries, "            ", rec.arg_count, rec.args_str, rec.has_self, pkg);
            result.registry_entries += "            let result = " + call_path + rec.name + "(";
            for (int i = 0; i < rec.arg_count; i++) {
                if (i > 0) result.registry_entries += ", ";
                result.registry_entries += "a" + std::to_string(i);
            }
            result.registry_entries += ")" + call_close + ";\n";
            RetCap ret_cap = return_strategy(rec.return_type);
            bool is_ptr = is_raw_ptr_type(rec.return_type);
            if (rec.is_result_ret) {
                result.registry_entries += "            match result {\n";
                if (ret_cap == RET_DISPLAY || is_ptr) {
                    result.registry_entries += "                Ok(v) => Ok(serde_json::Value::String(v.to_string())),\n";
                } else if (ret_cap == RET_HANDLE) {
                    result.registry_entries += "                Ok(v) => Ok(serde_json::json!(Box::into_raw(Box::new(v)) as usize)),\n";
                } else {
                    result.registry_entries += "                Ok(v) => serde_json::to_value(v).map_err(|e| e.to_string()),\n";
                }
                result.registry_entries += "                Err(e) => Err(e.to_string()),\n";
                result.registry_entries += "            }\n";
            } else {
                if (ret_cap == RET_DISPLAY || is_ptr) {
                    result.registry_entries += "            Ok(serde_json::Value::String(result.to_string()))\n";
                } else if (ret_cap == RET_HANDLE) {
                    result.registry_entries += "            Ok(serde_json::json!(Box::into_raw(Box::new(result)) as usize))\n";
                } else {
                    result.registry_entries += "            Ok(serde_json::to_value(result).map_err(|e| e.to_string())?)\n";
                }
            }
            result.registry_entries += "        });\n";
        } else {
            /* In impl block */
            std::string crate_type = rec.impl_type;
            size_t ga = crate_type.find('<');
            bool type_had_generics = (ga != std::string::npos);
            if (ga != std::string::npos) crate_type = crate_type.substr(0, ga);
            size_t col = crate_type.rfind("::");
            if (col != std::string::npos) crate_type = crate_type.substr(col + 2);
            std::string safe_type = crate_type;
            FnRec mut_rec = rec; /* Mutable copy for optional concrete substitution */
            bool using_concrete_subst = false;

            /* Compute placeholder type for generic types (e.g. Mutex<T> → Mutex<()>) */
            std::string subst_type;
            if (type_had_generics) {
                subst_type = placeholder_type(rec.impl_type);
                if (subst_type.empty()) {
                    std::cout << "[bridge]     skip " << safe_type << "::" << rec.name << " (generic type)\n";
                    continue;
                }
                /* Check if this method references generic params in its signature */
                auto impl_params = extract_impl_type_params(rec.impl_type);
                /* Types whose generic params require trait bounds that () won't satisfy */
                static const char* bounded_generic_types[] = {
                    "Map", "HashMap", "BTreeMap", "IndexMap",
                    "Deserializer", "StreamDeserializer",
                    "LineColIterator", "IoRead", "SliceRead", "StrRead",
                    "Read", "Write",
                    "OccupiedEntry", "VacantEntry", "Entry",
                    "Iter", "IterMut", "IntoIter", "Keys", "Values", "ValuesMut",
                    "Serializer",
                    "PrettyFormatter", "CompactFormatter",
                    "Formatter",
                };
                bool has_trait_bounds = false;
                for (auto* bt : bounded_generic_types) {
                    if (safe_type == bt) { has_trait_bounds = true; break; }
                }
                if (has_trait_bounds) {
                    std::string concrete = lookup_concrete_type(pkg, safe_type);
                    if (!concrete.empty()) {
                        /* Parse concrete values */
                        std::vector<std::string> concrete_vals;
                        {
                            std::istringstream cs(concrete);
                            std::string v;
                            while (std::getline(cs, v, ',')) {
                                v.erase(0, v.find_first_not_of(" \t\r\n"));
                                v.erase(v.find_last_not_of(" \t\r\n") + 1);
                                concrete_vals.push_back(v);
                            }
                        }
                        auto subst_in_fn = [&](std::string& s) {
                            for (size_t pi = 0; pi < impl_params.size() && pi < concrete_vals.size(); pi++) {
                                for (size_t p = 0; (p = s.find(impl_params[pi], p)) != std::string::npos; ) {
                                    char before = (p > 0) ? s[p - 1] : ' ';
                                    char after = (p + impl_params[pi].size() < s.size())
                                        ? s[p + impl_params[pi].size()] : ' ';
                                    if (!isalnum((unsigned char)before) && before != '_' &&
                                        !isalnum((unsigned char)after) && after != '_' &&
                                        after != ':' && before != ':') {
                                        s.replace(p, impl_params[pi].size(), concrete_vals[pi]);
                                        p += concrete_vals[pi].size();
                                    } else {
                                        p += impl_params[pi].size();
                                    }
                                }
                            }
                        };
                        /* Replace each () placeholder in subst_type with concrete params */
                        std::string new_subst = subst_type;
                        size_t npos = new_subst.find("()");
                        for (size_t ci = 0; ci < concrete_vals.size() && npos != std::string::npos; ci++) {
                            new_subst.replace(npos, 2, concrete_vals[ci]);
                            npos = new_subst.find("()", npos + concrete_vals[ci].size());
                        }
                        if (new_subst.find("()") != std::string::npos) {
                            std::cout << "[bridge]     skip " << safe_type << "::" << rec.name
                                      << " (unmatched concrete params)\n";
                        } else {
                            std::cout << "[bridge]     gen  " << safe_type << "::" << rec.name
                                      << " (concrete subst: " << new_subst << ")\n";
                            subst_type = new_subst;
                            subst_in_fn(mut_rec.args_str);
                            subst_in_fn(mut_rec.return_type);
                            if (!method_references_generic_params(mut_rec.args_str, mut_rec.return_type, impl_params))
                                using_concrete_subst = true;
                            else
                                std::cout << "[bridge]     skip " << safe_type << "::" << rec.name
                                          << " (concrete subst but still has generic refs)\n";
                        }
                    }
                    if (!using_concrete_subst) {
                        std::cout << "[bridge]     skip " << safe_type << "::" << rec.name
                                  << " (bounded generic type)\n";
                        continue;
                    }
                } else {
                    /* Non-bounded generic: check if method references generic params */
                    auto impl_params = extract_impl_type_params(rec.impl_type);
                    if (method_references_generic_params(rec.args_str, rec.return_type, impl_params)) {
                        std::cout << "[bridge]     skip " << safe_type << "::" << rec.name
                                  << " (generic params in signature)\n";
                        continue;
                    }
                    std::cout << "[bridge]     gen  " << subst_type << "::" << rec.name << " (placeholder generics)\n";
                }
            }
            /* Skip methods that take self by value (can't use with &this_ref) */
            {
                const auto& args_to_check = using_concrete_subst ? mut_rec.args_str : rec.args_str;
                if (rec.has_self && takes_self_by_value(args_to_check)) {
                    std::cout << "[bridge]     skip " << safe_type << "::" << rec.name << " (self by value)\n";
                    continue;
                }
            }

            /* Skip concrete types in private sub-modules that can't be named at crate root */
            {
                static const char* inaccessible_concrete_types[] = {
                    "RawValue", "IoErrorKind", "ErrorKind",
                };
                bool skip_inaccessible = false;
                for (auto* ct : inaccessible_concrete_types) {
                    if (safe_type == ct) { skip_inaccessible = true; break; }
                }
                if (skip_inaccessible) {
                    std::cout << "[bridge]     skip " << safe_type << "::" << rec.name
                              << " (inaccessible type)\n";
                    continue;
                }
            }

            if (!types.count(safe_type)) {
                types[safe_type] = {crate_type, safe_type, subst_type, {}, {}};
            }
            if (rec.has_self) {
                types[safe_type].methods.push_back(using_concrete_subst ? mut_rec : rec);
            } else {
                types[safe_type].ctors.push_back(using_concrete_subst ? mut_rec : rec);
            }
        }
    }

    /* Generate type-based registries */
    for (auto& [safe, bucket] : types) {
        const std::string& crate_type = bucket.crate_type;
        std::string type_ref = bucket.subst_type.empty() ? crate_type : bucket.subst_type;
        /* Fix sub-module type paths (e.g. RawValue → serde_json::raw::RawValue) */
        type_ref = fix_module_path(type_ref, pkg);
        result.method_count += (int)bucket.methods.size() + (int)bucket.ctors.size();

        /* Method registry init (for type_registry) */
        if (!bucket.methods.empty()) {
            result.method_registry_init += "    // ── " + type_ref + " methods ──\n";
            result.method_registry_init += "    {\n";
            result.method_registry_init += "        let tn = \"" + safe + "\".to_string();\n";
            result.method_registry_init += "        let mut methods: HashMap<String, MethodFn> = HashMap::new();\n";
            for (const auto& m : bucket.methods) {
                std::string m_args = (m.arg_count == 0) ? "_args" : "args";
                result.method_registry_init += "        methods.insert(\"" + m.name + "\".to_string(), |this_ptr: *mut c_void, " + m_args + ": Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {\n";
                bool m_self_mut = m.has_self && (m.args_str.find("&mut self") != std::string::npos);
                result.method_registry_init += "            let this_ref = unsafe { &" + std::string(m_self_mut ? "mut " : "") + "*(this_ptr as *" + (m_self_mut ? "mut" : "const") + " " + type_ref + ") };\n";
                gen_deser_args(result.method_registry_init, "            ", m.arg_count, m.args_str, m.has_self, pkg);
                std::string ap = m.is_async ? "futures::executor::block_on(" : "";
                std::string as_ = m.is_async ? ")" : "";
                result.method_registry_init += "            let __result = " + ap + "this_ref." + m.name + "(";
                for (int i = 0; i < m.arg_count; i++) {
                    if (i > 0) result.method_registry_init += ", ";
                    result.method_registry_init += "a" + std::to_string(i);
                }
                result.method_registry_init += ")" + as_ + ";\n";
                RetCap m_ret_cap = return_strategy(m.return_type);
                bool m_is_ptr = is_raw_ptr_type(m.return_type);
                bool m_is_self_ref = is_self_ref_type(m.return_type);
                if (m.is_result_ret) {
                    result.method_registry_init += "            match __result {\n";
                    if (m_ret_cap == RET_DISPLAY || m_is_ptr || m_is_self_ref) {
                        result.method_registry_init += "                Ok(v) => Ok(serde_json::Value::String(v.to_string())),\n";
                    } else if (m_ret_cap == RET_HANDLE) {
                        result.method_registry_init += "                Ok(v) => Ok(serde_json::json!(Box::into_raw(Box::new(v)) as usize)),\n";
                    } else {
                        result.method_registry_init += "                Ok(v) => serde_json::to_value(&v).map_err(|e| e.to_string()),\n";
                    }
                    result.method_registry_init += "                Err(e) => Err(e.to_string()),\n";
                    result.method_registry_init += "            }\n";
                } else {
                    if (m_ret_cap == RET_DISPLAY || m_is_ptr || m_is_self_ref) {
                        result.method_registry_init += "            Ok(serde_json::Value::String(__result.to_string()))\n";
                    } else if (m_ret_cap == RET_HANDLE) {
                        result.method_registry_init += "            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))\n";
                    } else {
                        result.method_registry_init += "            serde_json::to_value(&__result).map_err(|e| e.to_string())\n";
                    }
                }
                result.method_registry_init += "        });\n";
            }
            result.method_registry_init += "        type_map.insert(tn, methods);\n";
            result.method_registry_init += "    }\n";
        }

        /* Constructor registry init (for ctor_registry) */
        if (!bucket.ctors.empty()) {
            result.ctor_registry_init += "    // ── " + type_ref + " constructors ──\n";
            result.ctor_registry_init += "    {\n";
            result.ctor_registry_init += "        let tn = \"" + safe + "\".to_string();\n";
            result.ctor_registry_init += "        let mut ctors: HashMap<String, CtorFn> = HashMap::new();\n";
            for (const auto& c : bucket.ctors) {
                std::string c_args_name = (c.arg_count == 0) ? "_args" : "args";
                result.ctor_registry_init += "        ctors.insert(\"" + c.name + "\".to_string(), |" + c_args_name + ": Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {\n";
                gen_deser_args(result.ctor_registry_init, "            ", c.arg_count, c.args_str, c.has_self, pkg);
                std::string ap2 = c.is_async ? "futures::executor::block_on(" : "";
                std::string as2 = c.is_async ? ")" : "";
                result.ctor_registry_init += "            let __val = " + ap2 + turbofish_type(type_ref) + "::" + c.name + "(";
                for (int i = 0; i < c.arg_count; i++) {
                    if (i > 0) result.ctor_registry_init += ", ";
                    result.ctor_registry_init += "a" + std::to_string(i);
                }
                result.ctor_registry_init += ")" + as2 + ";\n";
                if (c.is_result_ret) {
                    result.ctor_registry_init += "            match __val {\n";
                    result.ctor_registry_init += "                Ok(v) => Ok(Box::into_raw(Box::new(v)) as *mut c_void),\n";
                    result.ctor_registry_init += "                Err(e) => Err(e.to_string()),\n";
                    result.ctor_registry_init += "            }\n";
                } else {
                    result.ctor_registry_init += "            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)\n";
                }
                result.ctor_registry_init += "        });\n";
            }
            result.ctor_registry_init += "        ctor_map.insert(tn, ctors);\n";
            result.ctor_registry_init += "    }\n";
        }

        /* Drop registry init */
        result.drop_registry_init += "    m.insert(\"" + safe + "\".to_string(), {\n";
        result.drop_registry_init += "        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut " + type_ref + ")); };\n";
        result.drop_registry_init += "        f\n";
        result.drop_registry_init += "    });\n";

        /* .au entries */
        for (const auto& m : bucket.methods) {
            result.method_au_entries += "@cost(alloc)\n";
            result.method_au_entries += "extern \"cargo_" + pkg + "\" function "
                + pkg + "_" + safe + "_" + m.name
                + "(this: pointer, args: pointer) -> pointer\n";
        }
        for (const auto& c : bucket.ctors) {
            result.method_au_entries += "@cost(alloc)\n";
            result.method_au_entries += "extern \"cargo_" + pkg + "\" function "
                + pkg + "_" + safe + "_" + c.name
                + "(args: pointer) -> pointer\n";
        }
    }

    int total = result.fn_count + result.method_count;
    if (total > 0) {
        std::cout << "[bridge]   auto-discovered " << result.fn_count
                  << " free functions, " << result.method_count << " methods/constructors\n";
    } else {
        std::cout << "[bridge]   WARNING: no bridgeable functions found (check for cfg/platform filters)\n";
    }

    /* Cleanup temp files */
    fs::remove(tarball_path);
    fs::remove_all(extract_dir);
    return result;
}

void gen_cargo_rust_wrapper(const std::string& pkg, const std::string& ver,
                             const std::string& dir,
                             const CargoDiscovery& disc)
{
    /* Cargo.toml */
    {
        std::ostringstream toml;
        toml << "[package]\n";
        toml << "name = \"" << pkg << "_bridge\"\n";
        toml << "version = \"0.1.0\"\n";
        toml << "edition = \"2021\"\n\n";
        toml << "[lib]\n";
        toml << "crate-type = [\"cdylib\"]\n\n";
        toml << "[dependencies]\n";
        toml << "serde = \"1\"\n";
        if (pkg != "serde_json") {
            toml << "serde_json = \"1\"\n";
        }
        toml << "futures = \"0.3\"\n";
        if (!disc.required_features.empty()) {
            toml << pkg << " = { version = \"" << ver << "\", features = [";
            for (size_t i = 0; i < disc.required_features.size(); i++) {
                if (i > 0) toml << ", ";
                toml << "\"" << disc.required_features[i] << "\"";
            }
            toml << "] }\n";
            std::cout << "[bridge]   features: ";
            for (size_t i = 0; i < disc.required_features.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << disc.required_features[i];
            }
            std::cout << "\n";
        } else {
            toml << pkg << " = \"" << ver << "\"\n";
        }
        if (write_file(dir + "/Cargo.toml", toml.str()))
            std::cout << "[bridge]   " << dir << "/Cargo.toml\n";
    }

    /* src/lib.rs */
    {
        fs::create_directories(dir + "/src");
        std::ostringstream rs;

        /***** HEADER *****/
        rs << "// Auto-generated Cargo bridge for " << pkg << "@" << ver << "\n";
        rs << "// Provides free-function + type-instance FFI API\n\n";
        rs << "use std::ffi::{CStr, CString, c_void};\n";
        rs << "use std::collections::HashMap;\n";
        rs << "use std::sync::Mutex;\n";
        rs << "use std::cell::RefCell;\n\n";

        /***** LAST ERROR (thread-local) *****/
        rs << "thread_local! {\n";
        rs << "    static LAST_ERROR: RefCell<String> = const { RefCell::new(String::new()) };\n";
        rs << "}\n\n";
        rs << "fn set_last_error(s: String) {\n";
        rs << "    LAST_ERROR.with(|e| *e.borrow_mut() = s);\n";
        rs << "}\n\n";

        /***** HELPERS *****/
        rs << "fn __deser<T: serde::de::DeserializeOwned>(v: serde_json::Value) -> Result<T, String> {\n";
        rs << "    serde_json::from_value(v).map_err(|e| e.to_string())\n";
        rs << "}\n\n";



        /***** TYPE ALIASES *****/
        rs << "type RustFn = fn(Vec<serde_json::Value>) -> Result<serde_json::Value, String>;\n";
        rs << "type MethodFn = fn(*mut c_void, Vec<serde_json::Value>) -> Result<serde_json::Value, String>;\n";
        rs << "type CtorFn = fn(Vec<serde_json::Value>) -> Result<*mut c_void, String>;\n";
        rs << "type DropFn = unsafe fn(*mut c_void);\n\n";

        /***** FREE-FUNCTION REGISTRY *****/
        rs << "fn registry() -> &'static Mutex<HashMap<String, RustFn>> {\n";
        rs << "    static REG: std::sync::OnceLock<Mutex<HashMap<String, RustFn>>> = std::sync::OnceLock::new();\n";
        rs << "    REG.get_or_init(|| {\n";
        if (!disc.registry_entries.empty()) {
            rs << "        let mut m: HashMap<String, RustFn> = HashMap::new();\n";
            rs << disc.registry_entries << "\n";
        } else {
            rs << "        let m: HashMap<String, RustFn> = HashMap::new();\n";
        }
        rs << "        Mutex::new(m)\n";
        rs << "    })\n";
        rs << "}\n\n";

        /***** TYPE METHOD REGISTRY *****/
        rs << "fn type_registry() -> &'static Mutex<HashMap<String, HashMap<String, MethodFn>>> {\n";
        rs << "    static REG: std::sync::OnceLock<Mutex<HashMap<String, HashMap<String, MethodFn>>>> = std::sync::OnceLock::new();\n";
        rs << "    REG.get_or_init(|| {\n";
        if (!disc.method_registry_init.empty()) {
            rs << "        let mut type_map: HashMap<String, HashMap<String, MethodFn>> = HashMap::new();\n";
            rs << disc.method_registry_init << "\n";
        } else {
            rs << "        let type_map: HashMap<String, HashMap<String, MethodFn>> = HashMap::new();\n";
        }
        rs << "        Mutex::new(type_map)\n";
        rs << "    })\n";
        rs << "}\n\n";

        /***** CONSTRUCTOR REGISTRY *****/
        rs << "fn ctor_registry() -> &'static Mutex<HashMap<String, HashMap<String, CtorFn>>> {\n";
        rs << "    static REG: std::sync::OnceLock<Mutex<HashMap<String, HashMap<String, CtorFn>>>> = std::sync::OnceLock::new();\n";
        rs << "    REG.get_or_init(|| {\n";
        if (!disc.ctor_registry_init.empty()) {
            rs << "        let mut ctor_map: HashMap<String, HashMap<String, CtorFn>> = HashMap::new();\n";
            rs << disc.ctor_registry_init;
        } else {
            rs << "        let ctor_map: HashMap<String, HashMap<String, CtorFn>> = HashMap::new();\n";
        }
        rs << "        Mutex::new(ctor_map)\n";
        rs << "    })\n";
        rs << "}\n\n";

        /***** DROP REGISTRY *****/
        rs << "fn drop_registry() -> &'static Mutex<HashMap<String, DropFn>> {\n";
        rs << "    static REG: std::sync::OnceLock<Mutex<HashMap<String, DropFn>>> = std::sync::OnceLock::new();\n";
        rs << "    REG.get_or_init(|| {\n";
        if (!disc.drop_registry_init.empty()) {
            rs << "        let mut m: HashMap<String, DropFn> = HashMap::new();\n";
            rs << disc.drop_registry_init;
        } else {
            rs << "        let m: HashMap<String, DropFn> = HashMap::new();\n";
        }
        rs << "        Mutex::new(m)\n";
        rs << "    })\n";
        rs << "}\n\n";

        /***** VALUE HELPERS *****/
        rs << "fn store(v: serde_json::Value) -> *mut c_void {\n";
        rs << "    Box::into_raw(Box::new(v)) as *mut c_void\n";
        rs << "}\n\n";
        rs << "unsafe fn retrieve<'a>(ptr: *mut c_void) -> &'a mut serde_json::Value {\n";
        rs << "    &mut *(ptr as *mut serde_json::Value)\n";
        rs << "}\n\n";

        /***** _import *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_import() -> *mut c_void {\n";
        rs << "    registry(); // ensure initialized\n";
        rs << "    store(serde_json::Value::Object(serde_json::Map::new()))\n";
        rs << "}\n\n";

        /***** _free (Value handle) *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_free(ptr: *mut c_void) {\n";
        rs << "    if !ptr.is_null() {\n";
        rs << "        unsafe { drop(Box::from_raw(ptr as *mut serde_json::Value)); }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _free_cstr *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_free_cstr(ptr: *mut std::ffi::c_char) {\n";
        rs << "    if !ptr.is_null() {\n";
        rs << "        unsafe { drop(CString::from_raw(ptr)); }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** VALUE CONVERTERS *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_str(s: *const std::ffi::c_char) -> *mut c_void {\n";
        rs << "    let s = unsafe { CStr::from_ptr(s) }.to_string_lossy().to_string();\n";
        rs << "    store(serde_json::Value::String(s))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_int(v: i64) -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Number(v.into()))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_float(v: f64) -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Number(\n";
        rs << "        serde_json::Number::from_f64(v).unwrap_or(serde_json::Number::from(0))))\n";
        rs << "}\n\n";

        /***** _tuple *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_tuple(items: *mut *mut c_void, count: i32) -> *mut c_void {\n";
        rs << "    let mut vec = Vec::with_capacity(count as usize);\n";
        rs << "    for i in 0..count {\n";
        rs << "        let ptr = unsafe { *items.offset(i as isize) };\n";
        rs << "        if ptr.is_null() {\n";
        rs << "            vec.push(serde_json::Value::Null);\n";
        rs << "        } else {\n";
        rs << "            vec.push(unsafe { retrieve(ptr) }.clone());\n";
        rs << "        }\n";
        rs << "    }\n";
        rs << "    store(serde_json::Value::Array(vec))\n";
        rs << "}\n\n";

        /***** _list *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_list(items: *mut *mut c_void, count: i32) -> *mut c_void {\n";
        rs << "    let mut vec = Vec::with_capacity(count as usize);\n";
        rs << "    for i in 0..count {\n";
        rs << "        let ptr = unsafe { *items.offset(i as isize) };\n";
        rs << "        if ptr.is_null() {\n";
        rs << "            vec.push(serde_json::Value::Null);\n";
        rs << "        } else {\n";
        rs << "            vec.push(unsafe { retrieve(ptr) }.clone());\n";
        rs << "        }\n";
        rs << "    }\n";
        rs << "    store(serde_json::Value::Array(vec))\n";
        rs << "}\n\n";

        /***** _list2.._list4, _tuple2.._tuple4 *****/
        auto gen_tuple_like = [&](const std::string& name, int n, bool use_list) {
            rs << "#[no_mangle]\n";
            rs << "pub extern \"C\" fn " << pkg << "_" << name << "(";
            std::string args_list = "abcdef";
            for (int i = 0; i < n; i++) {
                if (i > 0) rs << ", ";
                rs << args_list[i] << ": *mut c_void";
            }
            rs << ") -> *mut c_void {\n";
            rs << "    store(serde_json::Value::Array(vec![\n";
            for (int i = 0; i < n; i++) {
                rs << "        if " << args_list[i] << ".is_null() { serde_json::Value::Null } else { unsafe { retrieve(" << args_list[i] << ") }.clone() },\n";
            }
            rs << "    ]))\n";
            rs << "}\n\n";
        };
        gen_tuple_like("tuple2", 2, false);
        gen_tuple_like("tuple3", 3, false);
        gen_tuple_like("tuple4", 4, false);
        gen_tuple_like("list2", 2, true);
        gen_tuple_like("list3", 3, true);
        gen_tuple_like("list4", 4, true);

        /***** _dict *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_dict() -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Object(serde_json::Map::new()))\n";
        rs << "}\n\n";

        /***** _dict_set *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_dict_set(d: *mut c_void, key: *const std::ffi::c_char, val: *mut c_void) -> i32 {\n";
        rs << "    if d.is_null() || key.is_null() { return -1; }\n";
        rs << "    let k = match unsafe { CStr::from_ptr(key) }.to_str() {\n";
        rs << "        Ok(s) => s.to_string(),\n";
        rs << "        Err(_) => return -1,\n";
        rs << "    };\n";
        rs << "    let v = if val.is_null() { serde_json::Value::Null } else { unsafe { retrieve(val) }.clone() };\n";
        rs << "    match unsafe { retrieve(d) } {\n";
        rs << "        serde_json::Value::Object(map) => { map.insert(k, v); 0 },\n";
        rs << "        _ => -1,\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _to_cstr *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_to_cstr(obj: *mut c_void) -> *mut std::ffi::c_char {\n";
        rs << "    if obj.is_null() { return std::ptr::null_mut(); }\n";
        rs << "    let val = unsafe { retrieve(obj) };\n";
        rs << "    let s = serde_json::to_string(val).unwrap_or_else(|_| \"null\".to_string());\n";
        rs << "    CString::new(s).unwrap_or_default().into_raw()\n";
        rs << "}\n\n";

        /***** _getattr *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_getattr(obj: *mut c_void, name: *const std::ffi::c_char) -> *mut c_void {\n";
        rs << "    if obj.is_null() || name.is_null() { return std::ptr::null_mut(); }\n";
        rs << "    let key = match unsafe { CStr::from_ptr(name) }.to_str() { Ok(s) => s, Err(_) => return std::ptr::null_mut() };\n";
        rs << "    match unsafe { retrieve(obj) } {\n";
        rs << "        serde_json::Value::Object(map) => match map.get(key) {\n";
        rs << "            Some(v) => store(v.clone()),\n";
        rs << "            None => std::ptr::null_mut(),\n";
        rs << "        },\n";
        rs << "        _ => std::ptr::null_mut(),\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _call (with error propagation) *****/
        auto gen_call_fn = [&](const std::string& name, const std::string& args_decl,
                                const std::string& args_vec_build) {
            rs << "#[no_mangle]\n";
            rs << "pub extern \"C\" fn " << pkg << "_" << name << "(\n";
            rs << "    _handle: *mut c_void,\n";
            rs << "    fn_name: *const std::ffi::c_char,\n";
            rs << args_decl;
            rs << ") -> *mut c_void {\n";
            rs << "    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {\n";
            rs << "        Ok(s) => s,\n";
            rs << "        Err(_) => return std::ptr::null_mut(),\n";
            rs << "    };\n";
            rs << "    let args_vec = " << args_vec_build << ";\n";
            rs << "    let reg = registry().lock().unwrap();\n";
            rs << "    match reg.get(name) {\n";
            rs << "        Some(f) => match f(args_vec) {\n";
            rs << "            Ok(v) => store(v),\n";
            rs << "            Err(e) => { set_last_error(e); std::ptr::null_mut() }\n";
            rs << "        },\n";
            rs << "        None => { set_last_error(format!(\"unknown function: {}\", name)); std::ptr::null_mut() }\n";
            rs << "    }\n";
            rs << "}\n\n";
        };

        gen_call_fn("call",
            "    args: *mut c_void,\n",
            "if args.is_null() { vec![] } else { match unsafe { retrieve(args) } { serde_json::Value::Array(a) => a.clone(), _ => { set_last_error(\"args not array\".into()); return std::ptr::null_mut(); } } }");

        gen_call_fn("call1",
            "    a: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }]");

        gen_call_fn("call2",
            "    a: *mut c_void, b: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }]");

        gen_call_fn("call3",
            "    a: *mut c_void, b: *mut c_void, c: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }]");

        gen_call_fn("call4",
            "    a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }, if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() }]");

        gen_call_fn("call5",
            "    a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void, e: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }, if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() }, if e.is_null() { serde_json::Value::Null } else { unsafe { retrieve(e) }.clone() }]");

        gen_call_fn("call6",
            "    a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void, e: *mut c_void, f: *mut c_void,\n",
            "vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }, if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() }, if e.is_null() { serde_json::Value::Null } else { unsafe { retrieve(e) }.clone() }, if f.is_null() { serde_json::Value::Null } else { unsafe { retrieve(f) }.clone() }]");

        /***** _call_kw (with error propagation) *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_call_kw(\n";
        rs << "    _handle: *mut c_void,\n";
        rs << "    fn_name: *const std::ffi::c_char,\n";
        rs << "    args: *mut c_void,\n";
        rs << "    kwargs: *mut c_void,\n";
        rs << ") -> *mut c_void {\n";
        rs << "    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {\n";
        rs << "        Ok(s) => s,\n";
        rs << "        Err(_) => { set_last_error(\"invalid fn_name\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let mut args_vec = if args.is_null() {\n";
        rs << "        vec![]\n";
        rs << "    } else {\n";
        rs << "        match unsafe { retrieve(args) } {\n";
        rs << "            serde_json::Value::Array(a) => a.clone(),\n";
        rs << "            _ => { set_last_error(\"args not array\".into()); return std::ptr::null_mut(); }\n";
        rs << "        }\n";
        rs << "    };\n";
        rs << "    if !kwargs.is_null() {\n";
        rs << "        args_vec.push(unsafe { retrieve(kwargs) }.clone());\n";
        rs << "    }\n";
        rs << "    let reg = registry().lock().unwrap();\n";
        rs << "    match reg.get(name) {\n";
        rs << "        Some(f) => match f(args_vec) { Ok(v) => store(v), Err(e) => { set_last_error(e); std::ptr::null_mut() } },\n";
        rs << "        None => { set_last_error(format!(\"unknown function: {}\", name)); std::ptr::null_mut() }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _construct *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_construct(\n";
        rs << "    type_name: *const std::ffi::c_char,\n";
        rs << "    ctor_name: *const std::ffi::c_char,\n";
        rs << "    args: *mut c_void,\n";
        rs << ") -> *mut c_void {\n";
        rs << "    let tn = match unsafe { CStr::from_ptr(type_name) }.to_str() {\n";
        rs << "        Ok(s) => s, Err(_) => { set_last_error(\"invalid type_name\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let cn = match unsafe { CStr::from_ptr(ctor_name) }.to_str() {\n";
        rs << "        Ok(s) => s, Err(_) => { set_last_error(\"invalid ctor_name\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let args_vec = if args.is_null() { vec![] } else {\n";
        rs << "        match unsafe { retrieve(args) } {\n";
        rs << "            serde_json::Value::Array(a) => a.clone(),\n";
        rs << "            _ => { set_last_error(\"args not array\".into()); return std::ptr::null_mut(); }\n";
        rs << "        }\n";
        rs << "    };\n";
        rs << "    let reg = ctor_registry().lock().unwrap();\n";
        rs << "    match reg.get(tn).and_then(|ctors| ctors.get(cn)) {\n";
        rs << "        Some(f) => match f(args_vec) {\n";
        rs << "            Ok(ptr) => ptr,\n";
        rs << "            Err(e) => { set_last_error(e); std::ptr::null_mut() }\n";
        rs << "        },\n";
        rs << "        None => { set_last_error(format!(\"no constructor {}::{}\", tn, cn)); std::ptr::null_mut() }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _call_method *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_call_method(\n";
        rs << "    handle: *mut c_void,\n";
        rs << "    type_name: *const std::ffi::c_char,\n";
        rs << "    method: *const std::ffi::c_char,\n";
        rs << "    args: *mut c_void,\n";
        rs << ") -> *mut c_void {\n";
        rs << "    if handle.is_null() { set_last_error(\"null handle\".into()); return std::ptr::null_mut(); }\n";
        rs << "    let tn = match unsafe { CStr::from_ptr(type_name) }.to_str() {\n";
        rs << "        Ok(s) => s, Err(_) => { set_last_error(\"invalid type_name\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let mn = match unsafe { CStr::from_ptr(method) }.to_str() {\n";
        rs << "        Ok(s) => s, Err(_) => { set_last_error(\"invalid method\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let args_vec = if args.is_null() { vec![] } else {\n";
        rs << "        match unsafe { retrieve(args) } {\n";
        rs << "            serde_json::Value::Array(a) => a.clone(),\n";
        rs << "            _ => { set_last_error(\"args not array\".into()); return std::ptr::null_mut(); }\n";
        rs << "        }\n";
        rs << "    };\n";
        rs << "    let reg = type_registry().lock().unwrap();\n";
        rs << "    match reg.get(tn).and_then(|methods| methods.get(mn)) {\n";
        rs << "        Some(f) => match f(handle, args_vec) {\n";
        rs << "            Ok(v) => store(v),\n";
        rs << "            Err(e) => { set_last_error(e); std::ptr::null_mut() }\n";
        rs << "        },\n";
        rs << "        None => { set_last_error(format!(\"no method {}::{}\", tn, mn)); std::ptr::null_mut() }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _free_type *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_free_type(handle: *mut c_void, type_name: *const std::ffi::c_char) {\n";
        rs << "    if handle.is_null() { return; }\n";
        rs << "    let tn = match unsafe { CStr::from_ptr(type_name) }.to_str() {\n";
        rs << "        Ok(s) => s,\n";
        rs << "        Err(_) => { set_last_error(\"invalid type_name\".into()); return; }\n";
        rs << "    };\n";
        rs << "    let reg = drop_registry().lock().unwrap();\n";
        rs << "    if let Some(drop_fn) = reg.get(tn) {\n";
        rs << "        unsafe { drop_fn(handle); }\n";
        rs << "    } else {\n";
        rs << "        set_last_error(format!(\"unknown type for drop: {}\", tn));\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _last_error *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_last_error() -> *mut std::ffi::c_char {\n";
        rs << "    LAST_ERROR.with(|e| {\n";
        rs << "        let s = e.borrow().clone();\n";
        rs << "        if s.is_empty() { std::ptr::null_mut() } else { CString::new(s).unwrap_or_default().into_raw() }\n";
        rs << "    })\n";
        rs << "}\n";

        if (write_file(dir + "/src/lib.rs", rs.str()))
            std::cout << "[bridge]   " << dir << "/src/lib.rs\n";
    }
}

/* ──── gen_cargo_manual_scaffold ──────────────────────────────
   Generates a compilable Cargo.toml + src/lib.rs scaffold for
   crates whose generic/type-parameter-heavy API cannot be
   auto-discovered by the generic wrapper generator.
   ──────────────────────────────────────────────────────────── */
void gen_cargo_manual_scaffold(const std::string& pkg, const std::string& ver,
                                const std::string& dir)
{
    /* Cargo.toml */
    {
        std::ostringstream toml;
        toml << "[package]\n";
        toml << "name = \"" << pkg << "_bridge\"\n";
        toml << "version = \"0.1.0\"\n";
        toml << "edition = \"2021\"\n\n";
        toml << "[lib]\n";
        toml << "crate-type = [\"cdylib\"]\n\n";
        toml << "[dependencies]\n";
        toml << "serde = \"1\"\n";
        if (pkg != "serde_json") {
            toml << "serde_json = \"1\"\n";
        }
        toml << pkg << " = \"" << ver << "\"\n\n";
        toml << "# ── Uncomment additional deps your wrapper needs:\n";
        toml << "# serde_bytes = \"0.11\"\n";
        toml << "# chrono = { version = \"0.4\", features = [\"serde\"] }\n";
        toml << "# uuid = { version = \"1\", features = [\"serde\"] }\n";
        if (write_file(dir + "/Cargo.toml", toml.str()))
            std::cout << "[bridge]   " << dir << "/Cargo.toml\n";
    }

    /* src/lib.rs */
    {
        fs::create_directories(dir + "/src");
        std::ostringstream rs;

        rs << "// Manual bridge scaffold for " << pkg << "@" << ver << "\n";
        rs << "// Fill in each #[no_mangle] extern \"C\" fn below with real logic.\n";
        rs << "// Each fn returns *mut c_void → a heap-allocated serde_json::Value.\n\n";
        rs << "use std::ffi::{CStr, CString, c_void};\n";
        rs << "use std::sync::Mutex;\n";
        rs << "use std::collections::HashMap;\n\n";

        /***** LAST ERROR *****/
        rs << "thread_local! {\n";
        rs << "    static LAST_ERROR: std::cell::RefCell<String> = const { std::cell::RefCell::new(String::new()) };\n";
        rs << "}\n";
        rs << "fn set_last_error(s: String) {\n";
        rs << "    LAST_ERROR.with(|e| *e.borrow_mut() = s);\n";
        rs << "}\n\n";

        /***** VALUE HELPERS *****/
        rs << "fn store(v: serde_json::Value) -> *mut c_void {\n";
        rs << "    Box::into_raw(Box::new(v)) as *mut c_void\n";
        rs << "}\n";
        rs << "unsafe fn retrieve<'a>(ptr: *mut c_void) -> &'a mut serde_json::Value {\n";
        rs << "    &mut *(ptr as *mut serde_json::Value)\n";
        rs << "}\n\n";

        /***** _import / _free *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_import() -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Object(serde_json::Map::new()))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_free(ptr: *mut c_void) {\n";
        rs << "    if !ptr.is_null() {\n";
        rs << "        unsafe { drop(Box::from_raw(ptr as *mut serde_json::Value)); }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** VALUE CONVERTERS *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_str(s: *const std::ffi::c_char) -> *mut c_void {\n";
        rs << "    let s = unsafe { CStr::from_ptr(s) }.to_string_lossy().to_string();\n";
        rs << "    store(serde_json::Value::String(s))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_int(v: i64) -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Number(v.into()))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_float(v: f64) -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Number(\n";
        rs << "        serde_json::Number::from_f64(v).unwrap_or(serde_json::Number::from(0))))\n";
        rs << "}\n\n";

        /***** _tuple helpers *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_tuple(items: *mut *mut c_void, count: i32) -> *mut c_void {\n";
        rs << "    let mut vec = Vec::with_capacity(count as usize);\n";
        rs << "    for i in 0..count {\n";
        rs << "        let ptr = unsafe { *items.offset(i as isize) };\n";
        rs << "        vec.push(if ptr.is_null() { serde_json::Value::Null } else { unsafe { retrieve(ptr) }.clone() });\n";
        rs << "    }\n";
        rs << "    store(serde_json::Value::Array(vec))\n";
        rs << "}\n\n";

        /***** _dict helpers *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_dict() -> *mut c_void {\n";
        rs << "    store(serde_json::Value::Object(serde_json::Map::new()))\n";
        rs << "}\n\n";

        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_dict_set(d: *mut c_void, key: *const std::ffi::c_char, val: *mut c_void) -> i32 {\n";
        rs << "    if d.is_null() || val.is_null() { return 0; }\n";
        rs << "    let k = match unsafe { CStr::from_ptr(key) }.to_str() { Ok(s) => s.to_string(), Err(_) => return 0 };\n";
        rs << "    let obj = unsafe { retrieve(d) };\n";
        rs << "    let v = unsafe { retrieve(val) }.clone();\n";
        rs << "    obj.as_object_mut().map(|o| { o.insert(k, v); 1 }).unwrap_or(0)\n";
        rs << "}\n\n";

        /***** TEMPLATE: _call — replace with real logic *****/
        rs << "/* ════════════════════════════════════════════════════════════\n";
        rs << "   MANUAL WRAPPERS — Replace these stubs with real FFI calls\n";
        rs << "   ════════════════════════════════════════════════════════════ */\n\n";

        rs << "/// Example: call a function that takes (a: i32, b: String) -> bool\n";
        rs << "/// Delete once you've adapted it to your actual API.\n";
        rs << "pub fn " << pkg << "_add_user(args: Vec<serde_json::Value>) -> Result<serde_json::Value, String> {\n";
        rs << "    // Deserialize positional args\n";
        rs << "    let name: String = serde_json::from_value(args.get(0).ok_or(\"arg0 missing\")?.clone())\n";
        rs << "        .map_err(|e| e.to_string())?;\n";
        rs << "    let age: i32 = serde_json::from_value(args.get(1).ok_or(\"arg1 missing\")?.clone())\n";
        rs << "        .map_err(|e| e.to_string())?;\n";

        rs << "    // TODO: call " << pkg << "::some_function(name, age);\n";
        rs << "    // For now, return a placeholder:\n";
        rs << "    Ok(serde_json::json!({\"id\": 1, \"name\": name, \"age\": age}))\n";
        rs << "}\n\n";

        /***** _call / _call1.._call6 dispatchers *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_call(_mod: *mut c_void, fn_name: *const std::ffi::c_char, args: *mut c_void) -> *mut c_void {\n";
        rs << "    let fname = match unsafe { CStr::from_ptr(fn_name) }.to_str() {\n";
        rs << "        Ok(s) => s, Err(_) => { set_last_error(\"invalid fn_name\".into()); return std::ptr::null_mut(); }\n";
        rs << "    };\n";
        rs << "    let vargs = if args.is_null() { Vec::new() } else { unsafe { retrieve(args) }.as_array().cloned().unwrap_or_default() };\n";
        rs << "    let result: Result<serde_json::Value, String> = match fname {\n";
        rs << "        \"add_user\" => " << pkg << "_add_user(vargs),\n";
        rs << "        // TODO: add more function name -> handler mappings\n";
        rs << "        _ => Err(format!(\"unknown function: {}\", fname)),\n";
        rs << "    };\n";
        rs << "    match result {\n";
        rs << "        Ok(v) => store(v),\n";
        rs << "        Err(e) => { set_last_error(e); std::ptr::null_mut() }\n";
        rs << "    }\n";
        rs << "}\n\n";

        /***** _last_error *****/
        rs << "#[no_mangle]\n";
        rs << "pub extern \"C\" fn " << pkg << "_last_error() -> *mut std::ffi::c_char {\n";
        rs << "    LAST_ERROR.with(|e| {\n";
        rs << "        let s = e.borrow().clone();\n";
        rs << "        if s.is_empty() { std::ptr::null_mut() } else { CString::new(s).unwrap_or_default().into_raw() }\n";
        rs << "    })\n";
        rs << "}\n";

        if (write_file(dir + "/src/lib.rs", rs.str()))
            std::cout << "[bridge]   " << dir << "/src/lib.rs\n";
    }

    /* README */
    {
        std::ostringstream readme;
        readme << "# " << pkg << " — Manual Cargo Bridge\n\n";
        readme << "This bridge was generated with `--manual` because " << pkg
               << " uses generic types, trait bounds, or conditional compilation\n";
        readme << "that the auto-generator cannot handle.\n\n";
        readme << "## To complete\n\n";
        readme << "1. Open `src/lib.rs` and replace the `add_user` example stub with real wrappers.\n";
        readme << "2. Update the `_call()` dispatcher to map function names to your handlers.\n";
        readme << "3. If the crate's types do not implement `serde::Serialize`/`serde::Deserialize`,\n";
        readme << "   add manual conversion helpers (e.g. via `.to_string()` / `.parse()`).\n";
        readme << "4. Run `cargo build --release` in this directory.\n";
        readme << "5. The resulting cdylib will be loaded by Aurora at runtime.\n\n";
        readme << "## Exported C API\n\n";
        readme << "All `#[no_mangle] pub extern \"C\" fn` symbols in `src/lib.rs` are FFI entry points.\n";
        readme << "The Aurora runtime calls:\n";
        readme << "- `" << pkg << "_import()` — initialize\n";
        readme << "- `" << pkg << "_call(mod, \"fn_name\", args)` — invoke a function\n";
        readme << "- `" << pkg << "_free(ptr)` — free a returned value\n";
        readme << "- `" << pkg << "_last_error()` — retrieve last error string\n\n";
        readme << "## Dependencies\n\n";
        readme << "Edit `Cargo.toml` to add any additional crate dependencies your wrappers need.\n";
        readme << "Keep `serde` and `serde_json` — they are required by the bridge runtime.\n";

        if (write_file(dir + "/README.md", readme.str()))
            std::cout << "[bridge]   " << dir << "/README.md\n";
    }

    std::cout << "[bridge] ✅ Manual cargo scaffold created for " << pkg << "@" << ver << "\n";
    std::cout << "[bridge]   Edit src/lib.rs, then run: cd " << dir << " && cargo build --release\n";
}
