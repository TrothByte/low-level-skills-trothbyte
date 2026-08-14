// GOOD: a forwarding reference (T&&) plus std::forward<T> preserves the
// caller's value category: lvalue in -> copy, rvalue in -> move.
#include <cstdio>
#include <string>
#include <utility>

void sink(const std::string& s) {
    std::printf("sink copied: %s\n", s.c_str());
}
void sink(std::string&& s) {
    std::printf("sink moved: %s\n", s.c_str());
}

template <class T>
void forwarder(T&& x) {
    sink(std::forward<T>(x));   // moves only when the caller passed an rvalue
}

int main() {
    std::string s = "hello";
    forwarder(s);                       // lvalue: copy overload
    forwarder(std::string("tmp"));      // rvalue: move overload
    return 0;
}
