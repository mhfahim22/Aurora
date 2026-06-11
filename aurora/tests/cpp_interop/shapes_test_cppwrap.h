#ifndef CPPWRAP_SHAPES_TEST_H
#define CPPWRAP_SHAPES_TEST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Point ── */
/* ZERO-COST: trivially copyable — passed by value */
typedef struct Point {
    double x;
    double y;
    double z;
} Point;

double Point_length(void* self);
Point Point_add(void* self, void* other);
/* static method */
Point Point_origin();
void* Point_new();

/* ── Shape ── */
/* NON-TRIVIAL: opaque handle — indirection cost */
typedef void* Shape;

/* constructor */
void* Shape_new();
/* destructor */
void Shape_delete(void* self);
/* virtual method: area (vtable dispatch) */
/* virtual method: perimeter (vtable dispatch) */
int64_t Shape_id(void* self);
/* static method */
int64_t Shape_shape_count();

/* ── Circle ── */
/* NON-TRIVIAL: opaque handle — indirection cost */
typedef void* Circle;

/* constructor */
void* Circle_create(double radius);
/* virtual method: area (vtable dispatch) */
/* virtual method: perimeter (vtable dispatch) */
double Circle_radius(void* self);
void* Circle_new();

/* ── StringBuffer ── */
/* NON-TRIVIAL: opaque handle — indirection cost */
typedef void* StringBuffer;

/* constructor */
void* StringBuffer_new();
/* constructor */
void* StringBuffer_create(const char* str);
/* destructor */
void StringBuffer_delete(void* self);
void StringBuffer_append(void* self, const char* str);
const char* StringBuffer_c_str(void* self);
int64_t StringBuffer_size(void* self);
void StringBuffer_clear(void* self);

#ifdef __cplusplus
}
#endif

#endif /* guard */
