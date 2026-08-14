// BAD: a virtual call from a constructor does not reach the derived override.
// Expected wrong output: "Base", not "Derived".
#include <cstdio>

struct Base {
    Base() { log(); }
    virtual void log() const { std::printf("Base\n"); }
};

struct Derived : Base {
    void log() const override { std::printf("Derived\n"); }
};

int main() {
    Derived d;
    (void)d;
    return 0;
}
