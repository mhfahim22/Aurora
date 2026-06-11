#pragma once
#include <cstdint>
#include <cmath>

/* ════════════════════════════════════════════════════════════
   Test C++ header for aurora-cppwrap
   Tests: trivially copyable struct, class with methods,
          virtual inheritance, templates, static methods
   ════════════════════════════════════════════════════════════ */

/* ── Zero-cost: trivially copyable struct ── */
struct Point {
    double x;
    double y;
    double z;

    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Point add(const Point& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    static Point origin() {
        return {0.0, 0.0, 0.0};
    }
};

/* ── Opaque handle: class with constructor/destructor ── */
class Shape {
public:
    Shape() : id_(next_id_++) {}
    virtual ~Shape() = default;

    virtual double area() const = 0;
    virtual double perimeter() const = 0;

    int64_t id() const { return id_; }

    static int64_t shape_count() { return next_id_; }

private:
    int64_t id_;
    static int64_t next_id_;
};

inline int64_t Shape::next_id_ = 0;

/* ── Concrete class inheriting from Shape ── */
class Circle : public Shape {
public:
    explicit Circle(double radius) : radius_(radius) {}

    double area() const override {
        return 3.14159265358979323846 * radius_ * radius_;
    }

    double perimeter() const override {
        return 2.0 * 3.14159265358979323846 * radius_;
    }

    double radius() const { return radius_; }

private:
    double radius_;
};

/* ── Template class ── */
template<typename T>
class Vector3 {
public:
    Vector3() : x_(0), y_(0), z_(0) {}
    Vector3(T x, T y, T z) : x_(x), y_(y), z_(z) {}

    T dot(const Vector3& other) const {
        return x_ * other.x_ + y_ * other.y_ + z_ * other.z_;
    }

    Vector3 cross(const Vector3& other) const {
        return {
            y_ * other.z_ - z_ * other.y_,
            z_ * other.x_ - x_ * other.z_,
            x_ * other.y_ - y_ * other.x_
        };
    }

    T x() const { return x_; }
    T y() const { return y_; }
    T z() const { return z_; }

private:
    T x_, y_, z_;
};

/* ── Simple non-virtual class (opaque handle) ── */
class StringBuffer {
public:
    StringBuffer() : data_(nullptr), size_(0), capacity_(0) {}
    explicit StringBuffer(const char* str);
    ~StringBuffer();

    void append(const char* str);
    const char* c_str() const;
    int64_t size() const { return size_; }
    void clear();

private:
    char* data_;
    int64_t size_;
    int64_t capacity_;
};
