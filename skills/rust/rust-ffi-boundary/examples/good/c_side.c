/*
 * C-side layout probe, mirroring examples/good/rust_layout.rs.
 * Build (strict): gcc -Wall -Wextra -Werror -O2 -c c_side.c
 * Run:           gcc -std=c11 c_side.c -o c_side && ./c_side
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "good_layout.h"
#include "good_enum.h"

struct with_bool {
    _Bool b;
    uint32_t v;
};

int main(void) {
    printf("header size=%zu align=%zu off_len=%zu\n",
           sizeof(struct header), _Alignof(struct header), offsetof(struct header, len));
    printf("status size=%zu align=%zu\n",
           sizeof(enum status), _Alignof(enum status));
    printf("with_bool size=%zu align=%zu off_v=%zu\n",
           sizeof(struct with_bool), _Alignof(struct with_bool), offsetof(struct with_bool, v));
    return 0;
}
