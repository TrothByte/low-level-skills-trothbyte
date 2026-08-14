// BAD: a destructor that throws. Destructors are noexcept by default, so the
// exception escapes into a noexcept context and std::terminate is called.
#include <cstdio>
#include <stdexcept>

struct Boom {
    ~Boom() { throw std::runtime_error("dtor failure"); }
};

int main() {
    try {
        Boom b;
        (void)b;
    } catch (...) {
        std::printf("caught\n");
    }
    return 0;
}
