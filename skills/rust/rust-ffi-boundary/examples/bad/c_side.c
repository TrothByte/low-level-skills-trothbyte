/*
 * C-side probe for the bad layout example.
 * Build (strict): gcc -Wall -Wextra -Werror -O2 -c c_side.c
 * Run:            gcc -std=c11 c_side.c -o c_side && ./c_side
 * Contrast with bad_layout.rs: C says size 9 / off_len 5,
 * Rust (repr(C)) says size 12 / off_len 8.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "bad_layout.h"

int main(void) {
    printf("msg(packed) size=%zu align=%zu off_len=%zu\n",
           sizeof(struct msg), _Alignof(struct msg), offsetof(struct msg, len));
    return 0;
}
