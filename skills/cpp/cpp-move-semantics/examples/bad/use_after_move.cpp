// BAD: use-after-move. Reading a moved-from std::string value is valid but
// unspecified; dereferencing a moved-from unique_ptr is a null dereference.
// GCC -Wall -Wextra -Werror does NOT diagnose either; clang-tidy
// bugprone-use-after-move does.
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

int main() {
    std::string s = "payload";
    std::string t = std::move(s);
    std::printf("moved-from string size: %zu (valid but unspecified)\n", s.size());

    std::unique_ptr<int> p = std::make_unique<int>(7);
    auto q = std::move(p);
    std::printf("q value: %d\n", *q);
    std::fflush(stdout);
    std::printf("deref moved-from unique_ptr: %d\n", *p);
    return 0;
}
