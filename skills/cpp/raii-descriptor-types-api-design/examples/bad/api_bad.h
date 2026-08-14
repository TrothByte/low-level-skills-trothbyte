// BAD: an API design that invites misuse.
#include <cstddef>
#include <cstdio>

// B1: raw int handle — every caller must remember to close.
int open_file_bad(const char *path);   // returns raw fd; who closes?

// B2: caller-managed buffer, leak on error path.
char *make_buf_bad(std::size_t n) {
    return new char[n];
}

// B3: int return + errno-style failure.
int read_file_bad(int fd, char *dst, std::size_t n); // -1 + global errno

// B4: default-copyable resource (double-close).
struct HandleBad {
    int fd;
    // compiler-generated copy ctor/assign → two owners of one fd
};
