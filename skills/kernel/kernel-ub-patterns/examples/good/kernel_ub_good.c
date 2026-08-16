/*
 * GOOD: kernel-UB-safe patterns, host-compiled with UBSan. Covers:
 *  (1) strict-aliasing-safe reads via memcpy;
 *  (2) container_of used only on a proven member relationship;
 *  (3) size arithmetic guarded by check_mul_overflow;
 *  (4) a stubbed __user copy with a checked return (annotations carried).
 *
 * Build: gcc -Wall -Wextra -Werror -O2 -fsanitize=undefined -fno-sanitize-recover=all \
 *        kernel_ub_good.c -o ubgood
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* (3) overflow-checked multiply (kernel-style helper, host version). */
static int check_mul_overflow(size_t a, size_t b, size_t *res) {
    if (a != 0 && b > SIZE_MAX / a) return 1;
    *res = a * b;
    return 0;
}

/* (2) container_of equivalent for a simple embedded list head. */
struct list_head { struct list_head *next, *prev; };
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

struct drv {
    int id;
    struct list_head node;   /* member embedded in struct */
};

/* (4) stubbed copy_from_user contract: returns bytes not copied. */
#define __user
static size_t copy_from_user(void *dst, const void __user *src, size_t n) {
    (void)src;
    memcpy(dst, src, n);
    return 0;  /* all copied in this host stub */
}

int main(void) {
    /* (1) aliasing-safe buffer read: memcpy, never *(uint32_t*)buf. */
    uint8_t buf[8] = {0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0};
    uint32_t v;
    memcpy(&v, buf, sizeof v);
    assert(v == 0x12345678);

    /* (2) container_of on a guaranteed member relationship. */
    struct drv d = { .id = 42 };
    struct list_head *member_ptr = &d.node;   /* derived from the member */
    struct drv *back = container_of(member_ptr, struct drv, node);
    assert(back == &d);

    /* (3) overflow-checked allocation size. */
    size_t count = 10;
    size_t sz;
    assert(check_mul_overflow(count, sizeof(uint64_t), &sz) == 0);
    size_t bad_count = SIZE_MAX / sizeof(uint64_t) + 1;
    assert(check_mul_overflow(bad_count, sizeof(uint64_t), &sz) == 1);

    /* (4) copy helper with checked return. */
    uint32_t user_val = 7;
    uint32_t kbuf = 0;
    size_t nleft = copy_from_user(&kbuf, &user_val, sizeof kbuf);
    assert(nleft == 0);
    assert(kbuf == 7);

    printf("PASS: aliasing-safe, container_of correct, overflow-checked\n");
    return 0;
}
