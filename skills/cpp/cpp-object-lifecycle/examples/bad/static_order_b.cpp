// BAD (TU 2 of the static initialization order fiasco):
// defines the global that static_order_a.cpp reads. Until B's constructor
// runs, g_b.v holds its zero-initialized value.
struct B {
    B() : v(42) {}
    int v;
};

B g_b;

int get_b() { return g_b.v; }
