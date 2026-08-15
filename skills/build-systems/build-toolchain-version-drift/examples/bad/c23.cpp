// intentionally incorrect
// Uses a C++23 feature but no explicit -std= is set anywhere. GCC's default
// mode (gnu++20 on this toolchain) accepts `if consteval` only as a silent
// downgrade warning, so the build "works" while semantics differ from what the
// author intended. Unpinned standards drift silently; pin one explicitly.
int classify(int x) {
    if consteval { return 1; } else { return x & 1; }
}

int main() { return classify(3) == 1 ? 0 : 1; }
