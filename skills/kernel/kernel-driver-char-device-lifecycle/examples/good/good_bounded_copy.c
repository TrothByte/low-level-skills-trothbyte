/*
 * GOOD: bounded, checked copy_from_user.
 * user_len is validated against the real capacity BEFORE the copy; the return
 * value (bytes NOT copied) is checked and surfaced as -EFAULT.
 */
#include "stubs.h"

int handle_user_copy(const unsigned char __user *user_ptr, size_t user_len) {
    unsigned char kbuf[32];

    if (user_len > sizeof(kbuf))
        return -EINVAL;

    unsigned long not_copied = copy_from_user_emu(kbuf, user_ptr, user_len);
    if (not_copied)
        return -EFAULT;

    return 0;
}
