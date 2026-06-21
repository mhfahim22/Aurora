#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* ── JSON Value Types ── */
#define JSON_NULL    0
#define JSON_NUM     1
#define JSON_STR     2
#define JSON_ARRAY   3
#define JSON_OBJECT  4
#define JSON_BOOL    5

typedef struct JsonValue {
    int type; /* 0=null, 1=num, 2=str, 3=array, 4=object, 5=bool */
    double num_val;
    char* str_val;
    struct JsonValue** items;
    char** keys;
    int count;
} JsonValue;

/* ── Parsing ── */
JsonValue* aurora_json_parse(const char* str);
char*      aurora_json_serialize(JsonValue* val);

/* ── Construction ── */
JsonValue* aurora_json_new();
JsonValue* aurora_json_new_num(double num);
JsonValue* aurora_json_new_str(const char* str);
JsonValue* aurora_json_new_array();
JsonValue* aurora_json_new_object();

/* ── Object access ── */
void      aurora_json_set(JsonValue* obj, const char* key, double num);
void      aurora_json_set_str(JsonValue* obj, const char* key, const char* str);
void      aurora_json_set_obj(JsonValue* obj, const char* key, JsonValue* val);
double    aurora_json_get_num(JsonValue* obj, const char* key);
char*     aurora_json_get_str(JsonValue* obj, const char* key);
JsonValue* aurora_json_get_obj(JsonValue* obj, const char* key);

/* ── Array access ── */
void      aurora_json_array_push(JsonValue* arr, JsonValue* val);
JsonValue* aurora_json_array_get(JsonValue* arr, int idx);
int       aurora_json_array_len(JsonValue* arr);

/* ── Cleanup ── */
void aurora_json_free(JsonValue* val);

#ifdef __cplusplus
}
#endif
