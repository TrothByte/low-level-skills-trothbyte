// BAD: a user-declared destructor suppresses the implicit move constructor
// ([class.copy.ctor] p6), so std::move(a) silently selects the copy
// constructor. No compiler warning fires; only instrumentation reveals it.
#include <cstdio>
#include <utility>

struct Buffer {
    int id = 0;
    Buffer() : id(1) { std::puts("default ctor"); }
    ~Buffer() { std::puts("dtor"); }                 // user-declared: kills moves
    Buffer(const Buffer& o) : id(o.id) {
        std::puts("copy ctor: std::move did NOT move");
    }
};

int main() {
    Buffer a;
    Buffer b(std::move(a));                          // copy ctor runs, not a move
    std::printf("b.id = %d\n", b.id);
    return 0;
}
