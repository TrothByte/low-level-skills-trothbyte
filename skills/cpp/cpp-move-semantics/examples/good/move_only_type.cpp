// GOOD: move-only types (copy deleted) are transferred with std::move or
// returned by value; the source is guaranteed empty for unique_ptr.
#include <cstdio>
#include <memory>
#include <utility>

std::unique_ptr<int> build(int v) {
    return std::make_unique<int>(v);    // return by value, guaranteed elision
}

int main() {
    auto p = build(7);
    auto q = std::move(p);              // ownership moves once
    std::printf("q=%d p=%p\n", *q, (void*)p.get());
    return 0;
}
