// GOOD: reset or overwrite a moved-from object before reading it again, and
// check a moved-from pointer for null before dereferencing.
#include <cstdio>
#include <memory>
#include <string>

int main() {
    std::string s = "payload";
    std::string t = std::move(s);
    std::printf("moved target: %s\n", t.c_str());

    s.clear();
    s = "reused";
    std::printf("reset source: %s\n", s.c_str());

    std::unique_ptr<int> p(new int(7));
    auto q = std::move(p);
    std::printf("q value: %d\n", *q);
    if (p != nullptr) {
        std::printf("p still owns: %d\n", *p);
    } else {
        std::printf("p is null after move; not dereferenced\n");
    }
    return 0;
}
