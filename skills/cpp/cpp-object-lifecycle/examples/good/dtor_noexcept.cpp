// GOOD: a destructor that cannot throw. Failure reporting happens in an explicit
// close() the caller invokes; the destructor only runs as a safety net and
// swallows nothing it cannot report safely.
#include <cstdio>
#include <stdexcept>

class Session {
public:
    bool close() noexcept {
        if (!closed_) {
            closed_ = true;
            std::printf("Session closed cleanly\n");
        }
        return true;
    }

    ~Session() noexcept {
        try {
            close();
        } catch (...) {
        }
    }

private:
    bool closed_ = false;
};

int main() {
    try {
        Session s;
        s.close();
    } catch (...) {
        std::printf("caught\n");
    }
    return 0;
}
