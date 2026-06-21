#include "std/collections.hpp"
#include "std/json.hpp"
#include "runtime/memory.hpp"
#include "runtime/string.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

/* ════════════════════════════════════════════════════════════
   Phase 2 — Collection Types Runtime
   ════════════════════════════════════════════════════════════ */

/* ── Simple dynamic array-based list ── */
struct List {
    long long* data;
    int cap;
    int len;
};

void* list_new() {
    List* l = (List*)aurora_alloc(sizeof(List));
    l->cap = 8;
    l->len = 0;
    l->data = (long long*)aurora_alloc(l->cap * sizeof(long long));
    return l;
}

void list_push(void* list, long long val) {
    List* l = (List*)list;
    if (l->len >= l->cap) {
        l->cap *= 2;
        l->data = (long long*)aurora_safe_realloc(l->data, l->cap * sizeof(long long));
    }
    l->data[l->len++] = val;
}

long long list_get(void* list, int idx) {
    List* l = (List*)list;
    if (idx < 0 || idx >= l->len) return 0;
    return l->data[idx];
}

int list_len(void* list) {
    return ((List*)list)->len;
}

void list_free(void* list) {
    List* l = (List*)list;
    aurora_free(l->data);
    aurora_free(l);
}

/* ── Simple map (linear scan, small scale) ── */
struct MapEntry {
    char key[64];
    long long val;
};

struct Map {
    MapEntry* entries;
    int cap;
    int len;
};

void* map_new() {
    Map* m = (Map*)aurora_alloc(sizeof(Map));
    m->cap = 16;
    m->len = 0;
    m->entries = (MapEntry*)aurora_alloc(m->cap * sizeof(MapEntry));
    return m;
}

void map_set(void* map, const char* key, long long val) {
    Map* m = (Map*)map;
    for (int i = 0; i < m->len; i++) {
        if (strcmp(m->entries[i].key, key) == 0) {
            m->entries[i].val = val;
            return;
        }
    }
    if (m->len >= m->cap) {
        m->cap *= 2;
        m->entries = (MapEntry*)aurora_safe_realloc(m->entries, m->cap * sizeof(MapEntry));
    }
    strncpy_s(m->entries[m->len].key, sizeof(m->entries[m->len].key), key, _TRUNCATE);
    m->entries[m->len].val = val;
    m->len++;
}

long long map_get(void* map, const char* key) {
    Map* m = (Map*)map;
    for (int i = 0; i < m->len; i++) {
        if (strcmp(m->entries[i].key, key) == 0)
            return m->entries[i].val;
    }
    return 0;
}

int map_has(void* map, const char* key) {
    Map* m = (Map*)map;
    for (int i = 0; i < m->len; i++) {
        if (strcmp(m->entries[i].key, key) == 0)
            return 1;
    }
    return 0;
}

void map_free(void* map) {
    Map* m = (Map*)map;
    aurora_free(m->entries);
    aurora_free(m);
}

void map_copy(void* dst, void* src) {
    Map* s = (Map*)src;
    for (int i = 0; i < s->len; i++) {
        map_set(dst, s->entries[i].key, s->entries[i].val);
    }
}

/* WARNING: returns raw char* pointers from map entries.
   These char* are valid only while the map is alive.
   Do not store or free them. */
void* map_keys(void* map) {
    Map* m = (Map*)map;
    void* list = list_new();
    for (int i = 0; i < m->len; i++) {
        list_push(list, (long long)(intptr_t)(m->entries[i].key));
    }
    return list;
}

/* ── Set ── */
struct Set {
    long long* data;
    int cap;
    int len;
};

void* set_new() {
    Set* s = (Set*)aurora_alloc(sizeof(Set));
    s->cap = 8;
    s->len = 0;
    s->data = (long long*)aurora_alloc(s->cap * sizeof(long long));
    return s;
}

void set_add(void* set, long long val) {
    Set* s = (Set*)set;
    for (int i = 0; i < s->len; i++)
        if (s->data[i] == val) return;
    if (s->len >= s->cap) {
        s->cap *= 2;
        s->data = (long long*)aurora_safe_realloc(s->data, s->cap * sizeof(long long));
    }
    s->data[s->len++] = val;
}

int set_has(void* set, long long val) {
    Set* s = (Set*)set;
    for (int i = 0; i < s->len; i++)
        if (s->data[i] == val) return 1;
    return 0;
}

void set_free(void* set) {
    Set* s = (Set*)set;
    aurora_free(s->data);
    aurora_free(s);
}

/* ── Vector (math) ── */
struct Vector {
    double x, y, z;
};

void* vector_new(double x, double y, double z) {
    Vector* v = (Vector*)aurora_alloc(sizeof(Vector));
    v->x = x; v->y = y; v->z = z;
    return v;
}

double vector_x(void* v) { return ((Vector*)v)->x; }
double vector_y(void* v) { return ((Vector*)v)->y; }
double vector_z(void* v) { return ((Vector*)v)->z; }

/* ── Stack ── */
struct Stack {
    long long* data;
    int cap;
    int len;
};

void* stack_new() {
    Stack* s = (Stack*)aurora_alloc(sizeof(Stack));
    s->cap = 8; s->len = 0;
    s->data = (long long*)aurora_alloc(s->cap * sizeof(long long));
    return s;
}

void stack_push(void* stack, long long val) {
    Stack* s = (Stack*)stack;
    if (s->len >= s->cap) {
        s->cap *= 2;
        s->data = (long long*)aurora_safe_realloc(s->data, s->cap * sizeof(long long));
    }
    s->data[s->len++] = val;
}

long long stack_pop(void* stack) {
    Stack* s = (Stack*)stack;
    if (s->len == 0) return 0;
    return s->data[--s->len];
}

int stack_empty(void* stack) { return ((Stack*)stack)->len == 0; }
void stack_free(void* stack) {
    Stack* s = (Stack*)stack;
    aurora_free(s->data); aurora_free(s);
}

/* ── Queue (ring buffer) ── */
struct Queue {
    long long* data;
    int cap;
    int head;
    int tail;
    int count;
};

void* queue_new() {
    Queue* q = (Queue*)aurora_alloc(sizeof(Queue));
    q->cap = 8; q->head = 0; q->tail = 0; q->count = 0;
    q->data = (long long*)aurora_alloc(q->cap * sizeof(long long));
    return q;
}

void queue_enqueue(void* queue, long long val) {
    Queue* q = (Queue*)queue;
    if (q->count >= q->cap) {
        q->cap *= 2;
        long long* nd = (long long*)aurora_alloc(q->cap * sizeof(long long));
        for (int i = 0; i < q->count; i++)
            nd[i] = q->data[(q->head + i) % (q->cap / 2)];
        aurora_free(q->data);
        q->data = nd;
        q->head = 0;
        q->tail = q->count;
    }
    q->data[q->tail] = val;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;
}

long long queue_dequeue(void* queue) {
    Queue* q = (Queue*)queue;
    if (q->count == 0) return 0;
    long long val = q->data[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
    return val;
}

int queue_empty(void* queue) { return ((Queue*)queue)->count == 0; }
void queue_free(void* queue) {
    Queue* q = (Queue*)queue;
    aurora_free(q->data); aurora_free(q);
}

/* ── JSON (forwarding to std/json.cpp) ── */
/* These are forwarders called from generated code.
   The actual implementations are in aurora_json_* in std/json.cpp,
   but we provide simple wrappers here for the codegen bridge. */

void* json_new() {
    return aurora_json_new_object();
}

void* json_parse(const char* str) {
    return aurora_json_parse(str);
}

void json_set(void* j, const char* key, long long val) {
    aurora_json_set((JsonValue*)j, key, (double)val);
}

void* json_get(void* j, const char* key) {
    JsonValue* v = aurora_json_get_obj((JsonValue*)j, key);
    if (v && v->type == JSON_NUM) {
        /* Return the numeric value as a void* for codegen compatibility */
        return (void*)(intptr_t)(int64_t)v->num_val;
    }
    return (void*)v;
}

void json_free(void* j) {
    aurora_json_free((JsonValue*)j);
}
