#include <stdio.h>

int main(void) {
    int count = 0;
    long long total = 0;
    int n = 1000000000;

    for (int i = 0; i < n; i++) {
        count++;
        total += i;
    }

    printf("Loop ran %d times\n", count);
    printf("Sum total = %lld\n", total);
    printf("Expected  = 499999999500000000\n");
    return 0;
}
