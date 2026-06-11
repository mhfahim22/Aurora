#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct { char* ptr; size_t len; size_t cap; } AuroraStr;

AuroraStr* aurora_str_from_cstr(const char*);
void aurora_print_str(const char*);
void aurora_print_int(long long);

int main() {
    printf("Before aurora_str_from_cstr\n");
    AuroraStr* s = aurora_str_from_cstr("hello");
    printf("After: s=%p ptr=%p len=%zu\n", s, s ? s->ptr : 0, s ? s->len : 0);
    aurora_print_str((const char*)s);
    printf("After print\n");
    aurora_print_int(42);
    printf("After print_int\n");
    return 0;
}
