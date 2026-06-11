/* ════════════════════════════════════════════════════════════
   Auto-generated C++ extern "C" wrapper for: shapes_test
   ════════════════════════════════════════════════════════════ */

#include "shapes_test_cppwrap.h"
#include "..\aurora\tests\cpp_interop\test_shapes.hpp"

/* ════════════════════════════════════════════════════════════
   extern "C" thunks — one indirection per method call
   ════════════════════════════════════════════════════════════ */

double Point_length(void* self) {
    auto* obj = static_cast<Point*>(self);
    return obj->length();
}

Point Point_add(void* self, void* other) {
    auto* obj = static_cast<Point*>(self);
    return obj->add(other);
}

Point Point_origin() {
    return Point::origin();
}

void* Point_new() {
    return static_cast<void*>(new Point());
}

void* Shape_new() {
    return static_cast<void*>(new Shape());
}

void Shape_delete(void* self) {
    if (self) delete static_cast<Shape*>(self);
}

double Shape_area(void* self) {
    auto* obj = static_cast<Shape*>(self);
    return obj->area();
}

double Shape_perimeter(void* self) {
    auto* obj = static_cast<Shape*>(self);
    return obj->perimeter();
}

int64_t Shape_id(void* self) {
    auto* obj = static_cast<Shape*>(self);
    return obj->id();
}

int64_t Shape_shape_count() {
    return Shape::shape_count();
}

void* Circle_create(double radius) {
    return static_cast<void*>(new Circle(radius));
}

double Circle_area(void* self) {
    auto* obj = static_cast<Circle*>(self);
    return obj->area();
}

double Circle_perimeter(void* self) {
    auto* obj = static_cast<Circle*>(self);
    return obj->perimeter();
}

double Circle_radius(void* self) {
    auto* obj = static_cast<Circle*>(self);
    return obj->radius();
}

void* Circle_new() {
    return static_cast<void*>(new Circle());
}

void* StringBuffer_new() {
    return static_cast<void*>(new StringBuffer());
}

void* StringBuffer_create(const char* str) {
    return static_cast<void*>(new StringBuffer(str));
}

void StringBuffer_delete(void* self) {
    if (self) delete static_cast<StringBuffer*>(self);
}

void StringBuffer_append(void* self, const char* str) {
    auto* obj = static_cast<StringBuffer*>(self);
    return obj->append(str);
}

const char* StringBuffer_c_str(void* self) {
    auto* obj = static_cast<StringBuffer*>(self);
    return obj->c_str();
}

int64_t StringBuffer_size(void* self) {
    auto* obj = static_cast<StringBuffer*>(self);
    return obj->size();
}

void StringBuffer_clear(void* self) {
    auto* obj = static_cast<StringBuffer*>(self);
    return obj->clear();
}

