#include "runtime/template.hpp"
#include "std/json.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <sys/stat.h>
#include <ctime>

#if defined(_MSC_VER) && !defined(strdup)
#define strdup _strdup
#endif

/* ── Template AST node types ── */
enum class TplNodeType {
    Text,
    Variable,
    RawVariable,
    Section,
    InvertedSection,
    Partial,
    Comment
};

struct TplNode {
    TplNodeType type;
    std::string text;
    std::string key;
    std::vector<TplNode> children;
};

struct AuroraTemplate {
    std::string name;
    std::vector<TplNode> nodes;
};

/* ── File-backed template tracking for auto-reload ── */
struct TemplateFileInfo {
    std::string filepath;
    time_t mtime;
};

/* ── Global template registry ── */
static std::mutex g_tpl_mutex;
static std::unordered_map<std::string, AuroraTemplate*> g_templates;
static std::unordered_map<std::string, TemplateFileInfo> g_template_files;
static time_t g_template_cache_ttl = 5; /* seconds */

/* ── Forward declarations ── */
static std::vector<TplNode> parse_template(const char* src, size_t len, size_t& pos);
static std::string render_node(const TplNode& node, JsonValue* ctx);
static std::string html_escape(const std::string& s);
static JsonValue* lookup_json(JsonValue* ctx, const std::string& key);

/* ═══════════════════════════════════════════════════════════
   Parser
   ═══════════════════════════════════════════════════════════ */

static bool starts_with(const char* src, size_t pos, size_t len, const char* prefix) {
    size_t plen = strlen(prefix);
    if (pos + plen > len) return false;
    return memcmp(src + pos, prefix, plen) == 0;
}

static std::vector<TplNode> parse_template(const char* src, size_t len, size_t& pos) {
    std::vector<TplNode> nodes;
    std::string text_buf;
    size_t text_start = pos;

    while (pos < len) {
        if (starts_with(src, pos, len, "{{")) {
            if (pos > text_start) {
                text_buf.append(src + text_start, pos - text_start);
            }
            TplNode n;
            /* Check for raw {{{ }}} */
            if (pos + 2 < len && src[pos + 2] == '{') {
                /* {{{ ... }}} raw variable */
                size_t end = pos + 3;
                while (end + 2 < len && !(src[end] == '}' && src[end + 1] == '}' && src[end + 2] == '}')) {
                    end++;
                }
                if (end + 2 >= len) {
                    /* Unterminated {{{ — treat as text */
                    text_buf.append(src + pos, len - pos);
                    pos = len;
                    continue;
                }
                std::string raw(src + pos + 3, end - pos - 3);
                /* Trim */
                size_t a = raw.find_first_not_of(" \t\r\n");
                size_t b = raw.find_last_not_of(" \t\r\n");
                if (a != std::string::npos) raw = raw.substr(a, b - a + 1);
                else raw.clear();

                if (!raw.empty() && raw[0] == '!') {
                    n.type = TplNodeType::Comment;
                } else {
                    n.type = TplNodeType::RawVariable;
                    n.key = raw;
                }
                pos = end + 3;
            } else {
                /* {{ ... }} — check for special: #, ^, /, >, ! */
                size_t end = pos + 2;
                while (end + 1 < len && !(src[end] == '}' && src[end + 1] == '}')) {
                    end++;
                }
                if (end + 1 >= len) {
                    text_buf.append(src + pos, len - pos);
                    pos = len;
                    continue;
                }
                std::string tag(src + pos + 2, end - pos - 2);
                size_t ta = tag.find_first_not_of(" \t\r\n");
                size_t tb = tag.find_last_not_of(" \t\r\n");
                if (ta != std::string::npos) tag = tag.substr(ta, tb - ta + 1);
                else tag.clear();

                pos = end + 2;

                if (tag.empty()) continue;

                char first = tag[0];
                if (first == '!') {
                    n.type = TplNodeType::Comment;
                } else if (first == '#') {
                    n.type = TplNodeType::Section;
                    n.key = tag.substr(1);
                    /* Trim key */
                    size_t ka = n.key.find_first_not_of(" \t\r\n");
                    size_t kb = n.key.find_last_not_of(" \t\r\n");
                    if (ka != std::string::npos) n.key = n.key.substr(ka, kb - ka + 1);
                    /* Parse children until {{/key}} */
                    std::string end_tag = "/" + n.key;
                    while (pos < len) {
                        /* Look ahead for {{/key}} */
                        /* Find next {{ */
                        size_t next_mustache = len;
                        for (size_t i = pos; i + 1 < len; i++) {
                            if (src[i] == '{' && src[i + 1] == '{') {
                                next_mustache = i;
                                break;
                            }
                        }
                        if (next_mustache >= len) break;
                        /* Check if it's our end tag */
                        size_t et_end = next_mustache + 2;
                        while (et_end + 1 < len && !(src[et_end] == '}' && src[et_end + 1] == '}')) {
                            et_end++;
                        }
                        if (et_end + 1 >= len) break;
                        std::string etag(src + next_mustache + 2, et_end - next_mustache - 2);
                        size_t eta = etag.find_first_not_of(" \t\r\n");
                        size_t etb = etag.find_last_not_of(" \t\r\n");
                        if (eta != std::string::npos) etag = etag.substr(eta, etb - eta + 1);

                        if (etag == end_tag) {
                            pos = et_end + 2;
                            break;
                        }
                        n.children = parse_template(src, len, pos);
                    }
                } else if (first == '^') {
                    n.type = TplNodeType::InvertedSection;
                    n.key = tag.substr(1);
                    size_t ka = n.key.find_first_not_of(" \t\r\n");
                    size_t kb = n.key.find_last_not_of(" \t\r\n");
                    if (ka != std::string::npos) n.key = n.key.substr(ka, kb - ka + 1);
                    std::string end_tag = "/" + n.key;
                    while (pos < len) {
                        size_t next_mustache = len;
                        for (size_t i = pos; i + 1 < len; i++) {
                            if (src[i] == '{' && src[i + 1] == '{') {
                                next_mustache = i;
                                break;
                            }
                        }
                        if (next_mustache >= len) break;
                        size_t et_end = next_mustache + 2;
                        while (et_end + 1 < len && !(src[et_end] == '}' && src[et_end + 1] == '}')) {
                            et_end++;
                        }
                        if (et_end + 1 >= len) break;
                        std::string etag(src + next_mustache + 2, et_end - next_mustache - 2);
                        size_t eta = etag.find_first_not_of(" \t\r\n");
                        size_t etb = etag.find_last_not_of(" \t\r\n");
                        if (eta != std::string::npos) etag = etag.substr(eta, etb - eta + 1);
                        if (etag == end_tag) {
                            pos = et_end + 2;
                            break;
                        }
                        n.children = parse_template(src, len, pos);
                    }
                } else if (first == '>') {
                    n.type = TplNodeType::Partial;
                    n.key = tag.substr(1);
                    size_t ka = n.key.find_first_not_of(" \t\r\n");
                    size_t kb = n.key.find_last_not_of(" \t\r\n");
                    if (ka != std::string::npos) n.key = n.key.substr(ka, kb - ka + 1);
                } else if (first == '/') {
                    /* Closing tag encountered without matching open — stop */
                    break;
                } else {
                    n.type = TplNodeType::Variable;
                    n.key = tag;
                }
            }

            if (!text_buf.empty()) {
                TplNode text_node;
                text_node.type = TplNodeType::Text;
                text_node.text = text_buf;
                nodes.push_back(text_node);
                text_buf.clear();
            }
            if (n.type != TplNodeType::Comment) {
                nodes.push_back(n);
            }
            text_start = pos;
        } else {
            pos++;
        }
    }

    if (pos > text_start) {
        text_buf.append(src + text_start, pos - text_start);
    }
    if (!text_buf.empty()) {
        TplNode text_node;
        text_node.type = TplNodeType::Text;
        text_node.text = text_buf;
        nodes.push_back(text_node);
    }

    return nodes;
}

/* ═══════════════════════════════════════════════════════════
   JSON value lookup (dot-separated keys)
   ═══════════════════════════════════════════════════════════ */

static JsonValue* lookup_json(JsonValue* ctx, const std::string& key) {
    if (!ctx) return nullptr;

    /* Handle current context item */
    if (key == "." || key == "this") return ctx;

    /* Split by dot for nested access */
    size_t dot = key.find('.');
    if (dot == std::string::npos) {
        return aurora_json_get_obj(ctx, key.c_str());
    }

    std::string first = key.substr(0, dot);
    std::string rest = key.substr(dot + 1);
    JsonValue* child = aurora_json_get_obj(ctx, first.c_str());
    if (!child) return nullptr;
    return lookup_json(child, rest);
}

/* ═══════════════════════════════════════════════════════════
   HTML escape
   ═══════════════════════════════════════════════════════════ */

static std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default:   out += c; break;
        }
    }
    return out;
}

/* ═══════════════════════════════════════════════════════════
   Render a node to string
   ═══════════════════════════════════════════════════════════ */

static std::string render_node(const TplNode& node, JsonValue* ctx) {
    switch (node.type) {
        case TplNodeType::Text:
            return node.text;

        case TplNodeType::Variable: {
            JsonValue* val = lookup_json(ctx, node.key);
            if (!val) return "";
            if (val->type == JSON_NUM) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.17g", val->num_val);
                return html_escape(buf);
            } else if (val->type == JSON_STR && val->str_val) {
                return html_escape(val->str_val);
            } else if (val->type == JSON_BOOL) {
                return html_escape(val->num_val != 0 ? "true" : "false");
            }
            return "";
        }

        case TplNodeType::RawVariable: {
            JsonValue* val = lookup_json(ctx, node.key);
            if (!val) return "";
            if (val->type == JSON_NUM) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.17g", val->num_val);
                return buf;
            } else if (val->type == JSON_STR && val->str_val) {
                return val->str_val;
            } else if (val->type == JSON_BOOL) {
                return val->num_val != 0 ? "true" : "false";
            }
            return "";
        }

        case TplNodeType::Section: {
            JsonValue* val = lookup_json(ctx, node.key);
            if (!val) return "";
            std::string result;
            if (val->type == JSON_ARRAY) {
                int len = aurora_json_array_len(val);
                for (int i = 0; i < len; i++) {
                    JsonValue* item = aurora_json_array_get(val, i);
                    for (const auto& child : node.children) {
                        result += render_node(child, item ? item : val);
                    }
                }
            } else if (val->type == JSON_OBJECT) {
                for (const auto& child : node.children) {
                    result += render_node(child, val);
                }
            } else if (val->type == JSON_NUM) {
                if (val->num_val != 0.0) {
                    for (const auto& child : node.children) {
                        result += render_node(child, ctx);
                    }
                }
            } else if (val->type == JSON_STR && val->str_val && val->str_val[0] != '\0') {
                for (const auto& child : node.children) {
                    result += render_node(child, ctx);
                }
            } else if (val->type == JSON_BOOL && val->num_val != 0) {
                for (const auto& child : node.children) {
                    result += render_node(child, ctx);
                }
            }
            return result;
        }

        case TplNodeType::InvertedSection: {
            JsonValue* val = lookup_json(ctx, node.key);
            bool render = false;
            if (!val) {
                render = true;
            } else if (val->type == JSON_ARRAY && aurora_json_array_len(val) == 0) {
                render = true;
            } else if (val->type == JSON_NUM && val->num_val == 0.0) {
                render = true;
            } else if (val->type == JSON_STR && (!val->str_val || val->str_val[0] == '\0')) {
                render = true;
            } else if (val->type == JSON_BOOL && val->num_val == 0) {
                render = true;
            } else if (val->type == JSON_NULL) {
                render = true;
            }
            if (render) {
                std::string result;
                for (const auto& child : node.children) {
                    result += render_node(child, ctx);
                }
                return result;
            }
            return "";
        }

        case TplNodeType::Partial: {
            /* Look up partial from template registry */
            std::lock_guard<std::mutex> lock(g_tpl_mutex);
            auto it = g_templates.find(node.key);
            if (it != g_templates.end() && it->second) {
                std::string result;
                for (const auto& child : it->second->nodes) {
                    result += render_node(child, ctx);
                }
                return result;
            }
            return "";
        }

        case TplNodeType::Comment:
            return "";
    }
    return "";
}

/* ═══════════════════════════════════════════════════════════
   Public API
   ═══════════════════════════════════════════════════════════ */

extern "C" {

static bool check_template_reload(const char* name) {
    auto it = g_template_files.find(name);
    if (it == g_template_files.end()) return false;
    struct stat st;
    if (stat(it->second.filepath.c_str(), &st) != 0) return false;
    if (st.st_mtime <= it->second.mtime) return false;
    FILE* f = fopen(it->second.filepath.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string data((size_t)len, '\0');
    fread(&data[0], 1, (size_t)len, f);
    fclose(f);
    it->second.mtime = st.st_mtime;
    size_t pos = 0;
    auto nodes = parse_template(data.c_str(), data.size(), pos);
    AuroraTemplate* tpl = new AuroraTemplate();
    tpl->name = name;
    tpl->nodes = std::move(nodes);
    auto old = g_templates.find(name);
    if (old != g_templates.end()) delete old->second;
    g_templates[name] = tpl;
    return true;
}

AuroraTemplate* aurora_template_compile(const char* name, const char* source) {
    if (!name || !source) return nullptr;
    size_t len = strlen(source);
    size_t pos = 0;
    auto nodes = parse_template(source, len, pos);

    AuroraTemplate* tpl = new AuroraTemplate();
    tpl->name = name;
    tpl->nodes = std::move(nodes);

    std::lock_guard<std::mutex> lock(g_tpl_mutex);
    /* Free existing if any */
    auto it = g_templates.find(name);
    if (it != g_templates.end()) {
        delete it->second;
    }
    g_templates[name] = tpl;
    return tpl;
}

int aurora_template_compile_from_file(const char* name, const char* filepath) {
    if (!name || !filepath) return 0;
    FILE* f = fopen(filepath, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string data((size_t)len, '\0');
    fread(&data[0], 1, (size_t)len, f);
    fclose(f);
    AuroraTemplate* tpl = aurora_template_compile(name, data.c_str());
    if (!tpl) return 0;
    struct stat st;
    if (stat(filepath, &st) == 0) {
        std::lock_guard<std::mutex> lock(g_tpl_mutex);
        g_template_files[name] = {filepath, st.st_mtime};
    }
    return 1;
}

char* aurora_template_render(const char* name, const char* context_json) {
    if (!name) return nullptr;

    AuroraTemplate* tpl = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_tpl_mutex);
        auto it = g_templates.find(name);
        if (it != g_templates.end()) {
            tpl = it->second;
        }
    }
    if (!tpl) return nullptr;

    /* Auto-reload: check file mtime */
    {
        std::lock_guard<std::mutex> lock(g_tpl_mutex);
        check_template_reload(name);
        auto it = g_templates.find(name);
        if (it != g_templates.end()) tpl = it->second;
    }

    JsonValue* ctx = nullptr;
    if (context_json && context_json[0] != '\0') {
        ctx = aurora_json_parse(context_json);
    }
    if (!ctx) {
        /* Create empty object */
        ctx = aurora_json_new_object();
    }

    std::string result;
    for (const auto& node : tpl->nodes) {
        result += render_node(node, ctx);
    }

    aurora_json_free(ctx);
    return strdup(result.c_str());
}

char* aurora_template_render_to_string(const char* name, const char* context_json) {
    return aurora_template_render(name, context_json);
}

void aurora_template_free(const char* name) {
    if (!name) return;
    std::lock_guard<std::mutex> lock(g_tpl_mutex);
    auto it = g_templates.find(name);
    if (it != g_templates.end()) {
        delete it->second;
        g_templates.erase(it);
    }
    g_template_files.erase(name);
}

AuroraTemplate* aurora_template_register_string(const char* name, const char* source) {
    return aurora_template_compile(name, source);
}

char* aurora_template_render_string(const char* source, const char* context_json) {
    if (!source) return nullptr;
    size_t len = strlen(source);
    size_t pos = 0;
    auto nodes = parse_template(source, len, pos);

    JsonValue* ctx = nullptr;
    if (context_json && context_json[0] != '\0') {
        ctx = aurora_json_parse(context_json);
    }
    if (!ctx) {
        ctx = aurora_json_new_object();
    }

    std::string result;
    for (const auto& node : nodes) {
        result += render_node(node, ctx);
    }

    aurora_json_free(ctx);
    return strdup(result.c_str());
}

} /* extern "C" */
