/*
 * BAD: // intentionally incorrect — 2022 ChatGPT-demo class bug.
 * Unbounded copy_from_user into a 32-byte stack buffer: user_len is trusted,
 * the return value is ignored, and the partially/fully-overwritten buffer is
 * used. Compiles clean with plain gcc; the bug is a review-time catch.
 * Compile: gcc -Wall -Wextra -O2 bad_unbounded_copy.c
 */
#include "stubs.h"

struct request {
    unsigned char op;
    unsigned char data[31];
};

int handle_user_copy(const unsigned char __user *user_ptr, size_t user_len) {
    unsigned char stack_buf[32];
    unsigned long not_copied = copy_from_user_emu(stack_buf, user_ptr, user_len);
    if (not_copied)
        return -EFAULT;
    (void)stack_buf;
    return 0;
}
