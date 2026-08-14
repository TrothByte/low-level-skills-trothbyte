// GOOD: moved-from objects are reset before any read, and a moved-from
// unique_ptr is null-checked before dereference.
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

int main() {
    std::string s = "payload";
    std::string t = std::move(s);
    s = "fresh";                                    // reset before reading
    std::printf("t=%s s=%s\n", t.c_str(), s.c_str());

    std::unique_ptr<int> p = std::make_unique<int>(7);
    auto q = std::move(p);
    if (p) {
        std::printf("p holds %d\n", *p);
    } else {
        std::printf("p is null after move (checked before deref)\n");
    }
    std::printf("q value: %d\n", *q);
    return 0;
}
