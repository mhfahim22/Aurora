#include "..\aurora\tests\cpp_interop\shapes_test_cppwrap.h"
#include <cstdio>

int main() {
    /* Test Point (trivially copyable) */
    Point p = {1.0, 2.0, 3.0};
    printf("Point: %g %g %g\n", p.x, p.y, p.z);

    /* Test Circle (opaque handle) */
    void* c = Circle_create(5.0);
    double area = Circle_area(c);
    printf("Circle area: %g\n", area);
    Circle_delete(c);

    printf("All C++ interop tests passed!\n");
    return 0;
}
