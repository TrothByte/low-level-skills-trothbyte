// GOOD: buffer capacity passed explicitly instead of sizeof on a decayed pointer.
// Compile: gcc -Wall -Wextra -Werror -O2 -o sizeof_ptr_good sizeof_ptr_good.c && ./sizeof_ptr_good
#include <assert.h>
#include <string.h>

static char global_dst[64];

void fill(char *dst, size_t cap) {
    memset(dst, 'x', cap);              // size threaded explicitly
}

int main(void) {
    fill(global_dst, sizeof global_dst); // sizeof applied at the call site
    assert(global_dst[0] == 'x');
    assert(global_dst[63] == 'x');
    return 0;
}
