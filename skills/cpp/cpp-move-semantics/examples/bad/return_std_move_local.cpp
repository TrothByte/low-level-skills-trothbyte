// BAD: `return std::move(local)` blocks NRVO and forces a redundant move.
// GCC emits -Wpessimizing-move ("moving a local object in a return statement
// prevents copy elision"), so this fails to compile under -Werror.
#include <cstdio>
#include <string>
#include <utility>

std::string make() {
    std::string s = "payload";
    return std::move(s);      // anti-pattern: prevents elision, extra move
}

int main() {
    std::string s = make();
    std::printf("len: %zu\n", s.size());
    return 0;
}
