// GOOD: return the named local by value. At -O2 NRVO elides the move
// entirely (no ctor prints); with -fno-elide-constructors exactly one move
// runs. Never a copy, never std::move needed.
#include <cstdio>
#include <utility>

struct Widget {
    Widget() {}
    Widget(const Widget&) { std::puts("copy ctor"); }
    Widget(Widget&&) noexcept { std::puts("move ctor"); }
};

Widget make() {
    Widget w;                 // named local: NRVO-eligible
    return w;                 // elided at -O2, else moved, never copied
}

int main() {
    Widget w = make();        // guaranteed elision for the prvalue init
    (void)w;
    std::puts("done");
    return 0;
}
