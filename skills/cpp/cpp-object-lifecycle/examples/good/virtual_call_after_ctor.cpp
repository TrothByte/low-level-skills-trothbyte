// GOOD: call the virtual function only after construction completes, so
// dispatch reaches the most-derived override.
#include <cstdio>

struct Base {
    void init() { log(); }
    virtual void log() const { std::printf("Base\n"); }
};

struct Derived : Base {
    void log() const override { std::printf("Derived\n"); }
};

int main() {
    Derived d;
    d.init();
    return 0;
}
