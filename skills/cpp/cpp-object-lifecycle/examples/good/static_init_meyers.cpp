// GOOD: construct the global on first use (Meyers singleton), so the value is
// initialized before any consumer reads it regardless of translation unit order.
#include <cstdio>

struct B {
    B() : v(42) {}
    int v;
};

B& get_b() {
    static B b;
    return b;
}

struct A {
    A() {
        seen = get_b().v;
        std::printf("A ctor: read %d\n", seen);
    }
    int seen = -1;
};

A g_a;

int main() {
    std::printf("g_a.seen = %d\n", g_a.seen);
    return 0;
}
