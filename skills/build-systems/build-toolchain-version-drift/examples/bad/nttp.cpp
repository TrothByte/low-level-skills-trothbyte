// intentionally incorrect
// Built with -std=c++17 (or CXX_STANDARD 17 in CMake). The agent blames GCC
// for rejecting valid code; the real cause is the pinned language standard,
// and the error text names the required flag. Check the actual compile command
// before touching the source.
struct Point {
    int x, y;
    constexpr Point(int a, int b) : x(a), y(b) {}
};

// Class-type non-type template parameter: C++20 or later.
template<Point P>
int half() { return P.x; }

int main() { return half<Point{40, 2}>() == 40 ? 0 : 1; }
