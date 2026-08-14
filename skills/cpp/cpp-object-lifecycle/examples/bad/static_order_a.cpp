// BAD (TU 1 of the static initialization order fiasco):
// this global's constructor reads a global defined in static_order_b.cpp.
// Dynamic initialization order across translation units is unspecified,
// so g_a may run before B's constructor and observe the zero-initialized value.
#include <cstdio>

int get_b();

struct A {
    A() {
        seen = get_b();
        std::printf("A ctor: read %d\n", seen);
    }
    int seen = -1;
};

A g_a;

int main() { return 0; }
