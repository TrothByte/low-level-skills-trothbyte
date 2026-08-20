// GOOD: what a Checked C migration should ACHIEVE, in plain C that gcc compiles.
// This is the discipline-side twin of examples/good/annotated_checkedc.c:
// every buffer has an explicit capacity, every copy is length-checked, every
// index is range-checked. The Checked C annotations that would encode the same
// properties are shown in comments (gcc ignores them).
//
// Compile: gcc -Wall -Wextra -Werror -O2 -o incremental_step incremental_step.c && ./incremental_step
// Expect:  "incremental_step: all bounds respected", exit 0, no warnings.
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Documented Checked C annotation (what the migration targets):
//   void copy_range(_Array_ptr<const int> src : count(len),
//                   _Array_ptr<int> dst : count(len),
//                   size_t len);
static void copy_range(const int *src, int *dst, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        dst[i] = src[i];          // bounds-checked iteration: i in [0, len)
    }
}

// Documented Checked C annotation:
//   void copy_bytes(_Array_ptr<void> dst : byte_count(n),
//                   _Array_ptr<const void> src : byte_count(n),
//                   size_t n);
// The runtime guard below is what _Dynamic_check would emit.
static void copy_bytes(void *dst, const void *src, size_t n, size_t dst_cap) {
    assert(n <= dst_cap);         // the bound declaration's runtime check
    memcpy(dst, src, n);
}

int main(void) {
    int a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    int b[8] = {0};
    copy_range(a, b, 8);
    assert(b[7] == 8);

    // Capacity-aware string copy with an explicit length and terminator.
    char name[16];
    const char *user = "01234567890123456789";   // 20 chars + NUL
    size_t to_copy = strlen(user);
    if (to_copy >= sizeof name) {
        to_copy = sizeof name - 1;
    }
    copy_bytes(name, user, to_copy, sizeof name - 1);
    name[to_copy] = '\0';
    assert(strlen(name) <= sizeof name - 1);

    printf("incremental_step: all bounds respected\n");
    return 0;
}
