// Built with explicit -std=c++20. Diagnosis before blame: inspect the real
// compile command (ninja -t commands, CMake CXX_STANDARD, or `gcc -v`), then
// confirm the active standard with a macro dump:
//   gcc -x c++ -std=c++20 -dM -E - < /dev/null | grep __cplusplus   -> 202002L
struct Point {
    int x, y;
    constexpr Point(int a, int b) : x(a), y(b) {}
};

// Class-type non-type template parameter: C++20 or later.
template<Point P>
int half() { return P.x; }

int main() { return half<Point{40, 2}>() == 40 ? 0 : 1; }
