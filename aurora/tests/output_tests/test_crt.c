#include <stdlib.h>
#include <stdio.h>
int main() {
    void* p = malloc(100);
    printf("malloc returned %p\n", p);
    free(p);
    return 42;
}
