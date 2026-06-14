#include "bridge_cargo_impl.hpp"
#include "bridge_shared.h"
#include <set>
#include <cctype>
#include <regex>
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
    {"serde_json", "Serializer", "serde_json::Value"},
    {"serde_json", "Formatter", "serde_json::Value"},
    {"serde_json", "PrettyFormatter", "serde_json::Value"},
    {"serde_json", "CompactFormatter", "serde_json::Value"},
    /* serde: Serializer<...> trait with downstream concrete serializer */
    {"serde", "Serializer", "serde_json::Serializer<Vec<u8>>"},
    {"serde", "Deserializer", "serde_json::Deserializer<std::io::Cursor<Vec<u8>>>"},
    {"serde", "SerializeSeq", "serde_json::ser::SerializeSeq<serde_json::Serializer<Vec<u8>>>"},
    {"serde", "SerializeMap", "serde_json::ser::SerializeMap<serde_json::Serializer<Vec<u8>>>"},
    {"serde", "SerializeStruct", "serde_json::ser::SerializeStruct<serde_json::Serializer<Vec<u8>>>"},
    /* rand: distribution/RNG generics with trait bounds */
    {"rand", "DistIter", "rand::distributions::Standard, rand::rngs::ThreadRng"},
    {"rand", "Uniform", "f64"},
    {"rand", "WeightedIndex", "f64"},
    {"rand", "WeightedAliasIndex", "f64"},
    {"rand", "Alphanumeric", ""},
    {"rand", "Standard", ""},
    {"rand", "Open01", ""},
    {"rand", "OpenClosed01", ""},
    {"rand", "Bernoulli", ""},
    /* rand: RNG types (no bound generics, but included for completeness) */
    {"rand", "StdRng", ""},
    {"rand", "SmallRng", ""},
    {"rand", "ThreadRng", ""},
    /* regex: Match, Captures have lifetime params only; RegexSet/bytes are concrete */
    {"regex", "Match", "str"},
    {"regex", "Captures", "str"},
    {"regex", "SubCaptureMatches", "str"},
    {"regex", "Matches", "str, str"},
    {"regex", "Split", "str"},
    {"regex", "SplitN", "str"},
};

std::string lookup_concrete_type(const std::string& pkg,
                                          const std::string& base_type) {
    for (const auto& ct : known_concrete_types) {
        if (pkg == ct.crate_name && base_type == ct.base_type)
            return ct.concrete_params;
    }
    return {};
}

/* ── Helper function implementations ── */

bool is_test_cfg(const std::string& attr_body) {
    if (attr_body.find("not(") != std::string::npos) return false;
    return (attr_body == "cfg(test)" ||
            attr_body.find("(test)") != std::string::npos ||
            attr_body.find("(test,") != std::string::npos ||
            attr_body.find(",test,") != std::string::npos ||
            attr_body.find(",test)") != std::string::npos ||
            attr_body == "test");
}

bool parse_rust_fn(const std::string& content, size_t& offset,
                           std::string& name, std::string& args_str,
                           std::string& return_type,
                           bool& has_self, bool& is_async,
                           bool& has_generics,
                           std::set<std::string>* cfg_features,
                           bool* had_filtered_features,
                           bool* skip_platform)
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
std::string rust_escape(const std::string& s) {
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
int count_positional_args(const std::string& args_str) {
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
    std::string turbofish_type(const std::string& type_ref) {
        size_t ga = type_ref.find('<');
        if (ga == std::string::npos) return type_ref;
        return type_ref.substr(0, ga) + "::<" + type_ref.substr(ga + 1);
    }

    bool is_result_type(const std::string& rt) {
    if (rt.empty()) return false;
    if (rt == "Result") return true;
    if (rt.rfind("Result<", 0) == 0) return true;
    if (rt.rfind("std::result::Result<", 0) == 0) return true;
    return false;
}

/* ── Check if a feature is unsafe to auto-enable ── */
/* These features should be skipped: they either require nightly, are pseudo-features,
   require unstable cfg flags, or conflict with other detected features */
bool is_unsafe_feature(const std::string& feat) {
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
std::vector<std::string> split_cfg_args(const std::string& s) {
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
bool evaluate_cfg(const std::string& expr);

bool evaluate_simple_cfg(const std::string& s) {
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

bool evaluate_cfg(const std::string& expr) {
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
bool check_skip_due_to_platform_cfg(const std::string& attr_body) {
    /* Only handle #[cfg(...)] — NOT #[cfg_attr(...)] (which doesn't gate items) */
    if (attr_body.rfind("cfg(", 0) == 0 && attr_body.back() == ')') {
        std::string expr = attr_body.substr(4, attr_body.size() - 5);
        return !evaluate_cfg(expr);
    }
    return false;
}

/* ── Extract feature names from a #[cfg(...)] or #![cfg_attr(...)] attribute string ── */
void parse_cfg_features(const std::string& attr, std::set<std::string>& out,
                                bool* had_filtered) {
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
void gen_deser_args(std::string& s, const std::string& indent, int count,
                            const std::string& args_str,
                            bool has_self,
                            const std::string& pkg) {
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

bool is_raw_ptr_type(const std::string& rt) {
    if (rt.empty()) return false;
    size_t p = 0;
    while (p < rt.size() && isspace((unsigned char)rt[p])) p++;
    return (rt.compare(p, 5, "*mut ") == 0 || rt.compare(p, 7, "*const ") == 0);
}

    bool is_self_ref_type(const std::string& rt) {
        if (rt.empty()) return false;
        std::string trimmed = rt;
        size_t p = 0;
        while (p < trimmed.size() && isspace((unsigned char)trimmed[p])) p++;
        trimmed = trimmed.substr(p);
        return trimmed == "Self" || trimmed == "&Self" || trimmed == "&mut Self";
    }

    std::string placeholder_type(const std::string& impl_type) {
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

    bool is_never_type(const std::string& rt) {
        size_t p = 0;
        while (p < rt.size() && isspace((unsigned char)rt[p])) p++;
        return rt.compare(p, 1, "!") == 0;
    }

    /* Extract generic param names from impl type signature, e.g. Map<K, V> → {"K", "V"} */
    std::vector<std::string> extract_impl_type_params(const std::string& impl_type) {
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
    bool method_references_generic_params(const std::string& args_str,
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
    std::string fix_module_path(const std::string& type_name,
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
    bool takes_self_by_value(const std::string& args_str) {
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
/* Returns capability for a single type name (base, no &/mut/'lifetime) */
TypeCap type_capability(const std::string& base) {
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
bool is_non_serializable_type(const std::string& t) {
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
RetCap return_strategy(const std::string& t) {
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
ArgCap arg_strategy(const std::string& t) {
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
bool args_have_non_deserializable(const std::string& args_str) {
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

