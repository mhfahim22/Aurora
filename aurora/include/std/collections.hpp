#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ── List (dynamic array) ── */
void* list_new(void);
void  list_push(void* list, long long val);
long long list_get(void* list, int idx);
int   list_len(void* list);
void  list_free(void* list);

/* ── Map (hash table) ── */
void* map_new(void);
void  map_set(void* map, const char* key, long long val);
long long map_get(void* map, const char* key);
int   map_has(void* map, const char* key);
void  map_free(void* map);
void  map_copy(void* dst, void* src);
void* map_keys(void* map);

/* ── Set (hash set) ── */
void* set_new(void);
void  set_add(void* set, long long val);
int   set_has(void* set, long long val);
void  set_free(void* set);

/* ── Vector (math) ── */
void* vector_new(double x, double y, double z);
double vector_x(void* v);
double vector_y(void* v);
double vector_z(void* v);

/* ── Stack ── */
void* stack_new(void);
void  stack_push(void* s, long long val);
long long stack_pop(void* s);
int   stack_empty(void* s);
void  stack_free(void* s);

/* ── Queue ── */
void* queue_new(void);
void  queue_enqueue(void* q, long long val);
long long queue_dequeue(void* q);
int   queue_empty(void* q);
void  queue_free(void* q);

/* ── JSON ── */
void* json_parse(const char* str);
void* json_new(void);
void  json_set(void* j, const char* key, long long val);
void* json_get(void* j, const char* key);
void  json_free(void* j);

#ifdef __cplusplus
}
#endif
