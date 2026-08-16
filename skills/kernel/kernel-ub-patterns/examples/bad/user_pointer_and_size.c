/*
 * BAD: // intentionally incorrect — direct dereference of a user-space
 * pointer (missing copy_from_user) and an unchecked size multiplication.
 * (1) __user pointers must be copied via copy_to/from_user and never
 * dereferenced in kernel space. (2) count * sizeof wraps for large count,
 * undersizing the allocation (heap overflow class).
 *
 * Build: gcc -Wall -Wextra -Werror -O2 -fsanitize=undefined size_overflow.c -o sizefail
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define __user

static void *malloc_like(size_t n) { (void)n; return (void *)1; }

/* // intentionally incorrect: dereferences a user pointer directly */
static int read_user_int(const int __user *p) {
    return *p;                     /* user memory as kernel pointer */
}

/* // intentionally incorrect: bare count * sizeof in 32-bit size_t */
static void *kmalloc_bad(size_t n) {
    size_t sz = n * sizeof(uint64_t);   /* wraps on 32-bit size_t */
    return sz ? malloc_like(sz) : 0;
}

int main(void) {
    int __user *up = (int __user *)0x100000;   /* fake user address */
    (void)read_user_int(up);                    /* kernel deref of user ptr */

    size_t huge = (size_t)0xFFFFFFFFu / 8 + 2;  /* wraps to ~2 on 32-bit */
    if (kmalloc_bad(huge))
        printf("BUG: undersized allocation accepted (wrapped size)\n");
    return 0;
}
