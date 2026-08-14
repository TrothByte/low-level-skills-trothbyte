// GOOD: debug-friendly build.
// Compile: gcc -g -O0 -o debug_friendly.exe debug_friendly.c
// At -O0 every local variable gets a stack slot, so each DW_TAG_variable
// carries DW_AT_location (DW_OP_fbreg + offset) and stays in scope.

#include <stddef.h>

int add(int a, int b) {
    int sum = a + b;          // stack slot, DW_AT_location present
    return sum;
}

int sum_range(int n) {
    int total = 0;
    for (int i = 1; i <= n; i++) {   // total and i both materialized in memory
        total += i;
    }
    return total;
}

struct point {
    int x;
    int y;
};

int manhattan(struct point p) {
    int dx = p.x < 0 ? -p.x : p.x;
    int dy = p.y < 0 ? -p.y : p.y;
    return dx + dy;
}

int main(int argc, char **argv) {
    int r = add(1, 2);
    int s = sum_range(10);
    struct point p = {3, -4};
    int m = manhattan(p);
    return r + s + m;
}
