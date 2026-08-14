// BAD: use-after-move. Reading a moved-from string's value is valid but
// unspecified; dereferencing a moved-from unique_ptr is a null dereference.
#include <cstdio>
#include <memory>
#include <string>

int main() {
    std::string s = "payload";
    std::string t = std::move(s);
    std::printf("moved-from string size: %zu\n", s.size());
    std::printf("moved target: %s\n", t.c_str());

    std::unique_ptr<int> p(new int(7));
    auto q = std::move(p);
    std::printf("q value: %d\n", *q);
    std::fflush(stdout);
    std::printf("deref moved-from unique_ptr: %d\n", *p);
    return 0;
}
