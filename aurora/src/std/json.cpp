#include "std/json.hpp"
#include "runtime/memory.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>

#define JSON_BOOL 5

extern "C" {

/* ── Internal parser state ── */
typedef struct {
    const char* p;
    int line;
} JsonParser;

static void json_skip_ws(JsonParser* ps) {
    while (*ps->p && (unsigned char)*ps->p <= 32) {
        if (*ps->p == '\n') ps->line++;
        ps->p++;
    }
}

static JsonValue* json_parse_value(JsonParser* ps);

static char* json_parse_string_raw(JsonParser* ps) {
    if (*ps->p != '"') return nullptr;
    ps->p++; /* skip opening quote */
    size_t cap = 64, len = 0;
    char* buf = (char*)aurora_alloc(cap);
    while (*ps->p && *ps->p != '"') {
        if (*ps->p == '\\') {
            ps->p++;
            switch (*ps->p) {
                case '"':  buf[len++] = '"';  break;
                case '\\': buf[len++] = '\\'; break;
                case '/':  buf[len++] = '/';  break;
                case 'b':  buf[len++] = '\b'; break;
                case 'f':  buf[len++] = '\f'; break;
                case 'n':  buf[len++] = '\n'; break;
                case 'r':  buf[len++] = '\r'; break;
                case 't':  buf[len++] = '\t'; break;
                case 'u': {
                    /* simple 4-digit hex unicode (ASCII-only) */
                    char hex[5] = {0};
                    for (int i = 0; i < 4; i++) {
                        ps->p++;
                        hex[i] = *ps->p;
                    }
                    buf[len++] = (char)strtol(hex, nullptr, 16);
                    break;
                }
                default:   buf[len++] = *ps->p; break;
            }
        } else {
            buf[len++] = *ps->p;
        }
        if (len >= cap - 1) {
            cap *= 2;
            buf = (char*)aurora_safe_realloc(buf, cap);
        }
        ps->p++;
    }
    if (*ps->p == '"') ps->p++; /* skip closing quote */
    buf[len] = '\0';
    return buf;
}

static JsonValue* json_parse_string(JsonParser* ps) {
    char* s = json_parse_string_raw(ps);
    if (!s) return nullptr;
    JsonValue* v = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
    v->type = JSON_STR;
    v->str_val = s;
    return v;
}

static JsonValue* json_parse_number(JsonParser* ps) {
    const char* start = ps->p;
    if (*ps->p == '-') ps->p++;
    while (isdigit((unsigned char)*ps->p)) ps->p++;
    if (*ps->p == '.') {
        ps->p++;
        while (isdigit((unsigned char)*ps->p)) ps->p++;
    }
    if (*ps->p == 'e' || *ps->p == 'E') {
        ps->p++;
        if (*ps->p == '+' || *ps->p == '-') ps->p++;
        while (isdigit((unsigned char)*ps->p)) ps->p++;
    }
    size_t len = (size_t)(ps->p - start);
    char* num_str = (char*)aurora_alloc(len + 1);
    memcpy(num_str, start, len);
    num_str[len] = '\0';
    JsonValue* v = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
    v->type = JSON_NUM;
    v->num_val = strtod(num_str, nullptr);
    aurora_free(num_str);
    return v;
}

static JsonValue* json_parse_keyword(JsonParser* ps) {
    if (strncmp(ps->p, "true", 4) == 0) {
        ps->p += 4;
        JsonValue* v = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
        v->type = JSON_BOOL;
        v->num_val = 1.0;
        return v;
    }
    if (strncmp(ps->p, "false", 5) == 0) {
        ps->p += 5;
        JsonValue* v = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
        v->type = JSON_BOOL;
        v->num_val = 0.0;
        return v;
    }
    if (strncmp(ps->p, "null", 4) == 0) {
        ps->p += 4;
        JsonValue* v = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
        v->type = JSON_NULL;
        return v;
    }
    return nullptr;
}

static JsonValue* json_parse_array(JsonParser* ps) {
    ps->p++; /* skip '[' */
    JsonValue* arr = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
    arr->type = JSON_ARRAY;
    json_skip_ws(ps);
    if (*ps->p == ']') { ps->p++; return arr; }
    while (1) {
        JsonValue* elem = json_parse_value(ps);
        if (elem) {
            arr->count++;
            arr->items = (JsonValue**)aurora_safe_realloc(arr->items, arr->count * sizeof(JsonValue*));
            arr->items[arr->count - 1] = elem;
        }
        json_skip_ws(ps);
        if (*ps->p == ']') { ps->p++; break; }
        if (*ps->p == ',') { ps->p++; json_skip_ws(ps); }
    }
    return arr;
}

static JsonValue* json_parse_object(JsonParser* ps) {
    ps->p++; /* skip '{' */
    JsonValue* obj = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
    obj->type = JSON_OBJECT;
    json_skip_ws(ps);
    if (*ps->p == '}') { ps->p++; return obj; }
    while (1) {
        json_skip_ws(ps);
        char* key = json_parse_string_raw(ps);
        if (!key) break;
        json_skip_ws(ps);
        if (*ps->p == ':') ps->p++;
        json_skip_ws(ps);
        JsonValue* val = json_parse_value(ps);
        if (val) {
            obj->count++;
            obj->keys = (char**)aurora_safe_realloc(obj->keys, obj->count * sizeof(char*));
            obj->items = (JsonValue**)aurora_safe_realloc(obj->items, obj->count * sizeof(JsonValue*));
            obj->keys[obj->count - 1] = key;
            obj->items[obj->count - 1] = val;
        } else {
            aurora_free(key);
        }
        json_skip_ws(ps);
        if (*ps->p == '}') { ps->p++; break; }
        if (*ps->p == ',') { ps->p++; }
    }
    return obj;
}

static JsonValue* json_parse_value(JsonParser* ps) {
    json_skip_ws(ps);
    if (!*ps->p) return nullptr;
    if (*ps->p == '"') return json_parse_string(ps);
    if (*ps->p == '{') return json_parse_object(ps);
    if (*ps->p == '[') return json_parse_array(ps);
    if (*ps->p == '-' || isdigit((unsigned char)*ps->p)) return json_parse_number(ps);
    return json_parse_keyword(ps);
}

/* ── Public API ── */

JsonValue* aurora_json_parse(const char* str) {
    if (!str) return nullptr;
    JsonParser ps;
    ps.p = str;
    ps.line = 1;
    return json_parse_value(&ps);
}

static void json_serialize_impl(JsonValue* val, char** buf, size_t* cap, size_t* len) {
    if (!val) { /* null */ }
    auto ensure = [&](size_t needed) {
        while (*len + needed + 1 >= *cap) {
            *cap *= 2;
            *buf = (char*)aurora_safe_realloc(*buf, *cap);
        }
    };
    switch (val->type) {
        case JSON_NULL: {
            ensure(5);
            memcpy(*buf + *len, "null", 4); *len += 4;
            break;
        }
        case JSON_BOOL: {
            const char* s = val->num_val != 0.0 ? "true" : "false";
            int n = (int)strlen(s);
            ensure((size_t)n);
            memcpy(*buf + *len, s, (size_t)n); *len += (size_t)n;
            break;
        }
        case JSON_NUM: {
            char tmp[64];
            int n = snprintf(tmp, sizeof(tmp), "%g", val->num_val);
            ensure((size_t)n);
            memcpy(*buf + *len, tmp, (size_t)n); *len += (size_t)n;
            break;
        }
        case JSON_STR: {
            ensure(strlen(val->str_val) + 3);
            (*buf)[(*len)++] = '"';
            for (char* s = val->str_val; *s; s++) {
                if (*s == '"' || *s == '\\') {
                    (*buf)[(*len)++] = '\\';
                }
                (*buf)[(*len)++] = *s;
            }
            (*buf)[(*len)++] = '"';
            break;
        }
        case JSON_ARRAY: {
            ensure(2);
            (*buf)[(*len)++] = '[';
            for (int i = 0; i < val->count; i++) {
                if (i > 0) { ensure(1); (*buf)[(*len)++] = ','; }
                json_serialize_impl(val->items[i], buf, cap, len);
            }
            ensure(1);
            (*buf)[(*len)++] = ']';
            break;
        }
        case JSON_OBJECT: {
            ensure(2);
            (*buf)[(*len)++] = '{';
            for (int i = 0; i < val->count; i++) {
                if (i > 0) { ensure(1); (*buf)[(*len)++] = ','; }
                ensure(strlen(val->keys[i]) + 3);
                (*buf)[(*len)++] = '"';
                for (char* s = val->keys[i]; *s; s++) {
                    if (*s == '"' || *s == '\\') (*buf)[(*len)++] = '\\';
                    (*buf)[(*len)++] = *s;
                }
                (*buf)[(*len)++] = '"';
                ensure(1); (*buf)[(*len)++] = ':';
                json_serialize_impl(val->items[i], buf, cap, len);
            }
            ensure(1);
            (*buf)[(*len)++] = '}';
            break;
        }
    }
    (*buf)[*len] = '\0';
}

char* aurora_json_serialize(JsonValue* val) {
    if (!val) return strdup("null");
    size_t cap = 256, len = 0;
    char* buf = (char*)aurora_alloc(cap);
    json_serialize_impl(val, &buf, &cap, &len);
    return buf;
}

JsonValue* aurora_json_new() {
    JsonValue* val = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
    if (val) val->type = JSON_NULL;
    return val;
}

JsonValue* aurora_json_new_num(double num) {
    JsonValue* val = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
    val->type = JSON_NUM;
    val->num_val = num;
    return val;
}

JsonValue* aurora_json_new_str(const char* str) {
    JsonValue* val = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
    val->type = JSON_STR;
    val->str_val = strdup(str ? str : "");
    return val;
}

JsonValue* aurora_json_new_array() {
    JsonValue* val = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
    val->type = JSON_ARRAY;
    return val;
}

JsonValue* aurora_json_new_object() {
    JsonValue* val = (JsonValue*)aurora_safe_calloc(1, sizeof(JsonValue));
    val->type = JSON_OBJECT;
    return val;
}

void aurora_json_set(JsonValue* obj, const char* key, double num) {
    if (!obj) return;
    if (obj->type != JSON_OBJECT && obj->type != JSON_ARRAY) {
        obj->type = JSON_OBJECT;
        obj->count = 0;
        obj->items = nullptr;
    }
    for (int i = 0; i < obj->count; i++) {
        if (obj->keys[i] && strcmp(obj->keys[i], key) == 0) {
            if (obj->items[i]) {
                obj->items[i]->type = JSON_NUM;
                obj->items[i]->num_val = num;
            }
            return;
        }
    }
    obj->count++;
    obj->keys = (char**)aurora_safe_realloc(obj->keys, obj->count * sizeof(char*));
    obj->items = (JsonValue**)aurora_safe_realloc(obj->items, obj->count * sizeof(JsonValue*));
    obj->keys[obj->count - 1] = strdup(key);
    obj->items[obj->count - 1] = aurora_json_new_num(num);
}

void aurora_json_set_str(JsonValue* obj, const char* key, const char* str) {
    if (!obj) return;
    obj->type = JSON_OBJECT;
    for (int i = 0; i < obj->count; i++) {
        if (obj->keys[i] && strcmp(obj->keys[i], key) == 0) {
            if (obj->items[i]) {
                free(obj->items[i]->str_val);
                obj->items[i]->type = JSON_STR;
                obj->items[i]->str_val = strdup(str ? str : "");
            }
            return;
        }
    }
    obj->count++;
    obj->keys = (char**)aurora_safe_realloc(obj->keys, obj->count * sizeof(char*));
    obj->items = (JsonValue**)aurora_safe_realloc(obj->items, obj->count * sizeof(JsonValue*));
    obj->keys[obj->count - 1] = strdup(key);
    obj->items[obj->count - 1] = aurora_json_new_str(str);
}

void aurora_json_set_obj(JsonValue* obj, const char* key, JsonValue* val) {
    if (!obj || !val) return;
    obj->type = JSON_OBJECT;
    for (int i = 0; i < obj->count; i++) {
        if (obj->keys[i] && strcmp(obj->keys[i], key) == 0) {
            if (obj->items[i]) aurora_json_free(obj->items[i]);
            obj->items[i] = val;
            return;
        }
    }
    obj->count++;
    obj->keys = (char**)aurora_safe_realloc(obj->keys, obj->count * sizeof(char*));
    obj->items = (JsonValue**)aurora_safe_realloc(obj->items, obj->count * sizeof(JsonValue*));
    obj->keys[obj->count - 1] = strdup(key);
    obj->items[obj->count - 1] = val;
}

double aurora_json_get_num(JsonValue* obj, const char* key) {
    if (!obj) return 0;
    for (int i = 0; i < obj->count; i++) {
        if (obj->keys[i] && strcmp(obj->keys[i], key) == 0) {
            if (obj->items[i] && obj->items[i]->type == JSON_NUM)
                return obj->items[i]->num_val;
        }
    }
    return 0;
}

char* aurora_json_get_str(JsonValue* obj, const char* key) {
    if (!obj) return nullptr;
    for (int i = 0; i < obj->count; i++) {
        if (obj->keys[i] && strcmp(obj->keys[i], key) == 0) {
            if (obj->items[i] && obj->items[i]->type == JSON_STR)
                return obj->items[i]->str_val;
        }
    }
    return nullptr;
}

JsonValue* aurora_json_get_obj(JsonValue* obj, const char* key) {
    if (!obj) return nullptr;
    for (int i = 0; i < obj->count; i++) {
        if (obj->keys[i] && strcmp(obj->keys[i], key) == 0)
            return obj->items[i];
    }
    return nullptr;
}

void aurora_json_array_push(JsonValue* arr, JsonValue* val) {
    if (!arr || !val) return;
    arr->type = JSON_ARRAY;
    arr->count++;
    arr->items = (JsonValue**)aurora_safe_realloc(arr->items, arr->count * sizeof(JsonValue*));
    arr->items[arr->count - 1] = val;
}

JsonValue* aurora_json_array_get(JsonValue* arr, int idx) {
    if (!arr || idx < 0 || idx >= arr->count) return nullptr;
    return arr->items[idx];
}

int aurora_json_array_len(JsonValue* arr) {
    return arr ? arr->count : 0;
}

void aurora_json_free(JsonValue* val) {
    if (!val) return;
    if (val->type == JSON_STR) {
        aurora_free(val->str_val);
    }
    if (val->type == JSON_OBJECT || val->type == JSON_ARRAY) {
        if (val->type == JSON_OBJECT && val->keys) {
            for (int i = 0; i < val->count; i++) {
                aurora_free(val->keys[i]);
            }
            aurora_free(val->keys);
        }
        for (int i = 0; i < val->count; i++) {
            aurora_json_free(val->items[i]);
        }
        aurora_free(val->items);
    }
    aurora_free(val);
}

}
