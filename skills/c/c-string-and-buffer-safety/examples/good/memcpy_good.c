// GOOD: memmove for possibly-overlapping regions; memcpy only when disjoint.
// Compile: gcc -Wall -Wextra -Werror -O2 -o memcpy_good memcpy_good.c && ./memcpy_good
#include <assert.h>
#include <string.h>

int main(void) {
    char buf[16];
    memcpy(buf, "abcdefghij", 11);      // disjoint source/destination: memcpy is fine
    memmove(buf + 1, buf, 10);          // overlapping: memmove required
    assert(memcmp(buf, "aabcdefghij", 11) == 0);
    return 0;
}
