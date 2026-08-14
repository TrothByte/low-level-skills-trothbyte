// GOOD: explicit noexcept move ctor transfers the resource and leaves the
// source in a valid, specified state (null). Print instrumentation proves the
// move ctor (not the copy ctor) runs; copying is deleted.
#include <cstdio>
#include <cstddef>
#include <utility>

struct Buffer {
    char* data = nullptr;
    size_t n = 0;
    Buffer() {}
    explicit Buffer(size_t sz) : n(sz) {}
    ~Buffer() { delete[] data; }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& o) noexcept : data(o.data), n(o.n) {
        std::puts("move ctor");
        o.data = nullptr;   // moved-from state is valid and specified: null
        o.n = 0;
    }
    Buffer& operator=(Buffer&& o) noexcept {
        std::puts("move assign");
        delete[] data;
        data = o.data;
        o.data = nullptr;
        n = o.n;
        o.n = 0;
        return *this;
    }
};

int main() {
    Buffer a(1000);
    Buffer b(std::move(a));                 // prints "move ctor"
    std::printf("a.data=%p b.n=%zu\n", (void*)a.data, b.n);
    b = Buffer(8);                          // prints "move assign"
    return 0;
}
