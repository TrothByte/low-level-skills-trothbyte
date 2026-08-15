/*
 * GOOD: correct read/write direction with checked copies.
 * read = device data out (copy_to_user); write = user data in (copy_from_user).
 * Both return the count or -EFAULT, never a partial-success.
 */
#include "stubs.h"

static unsigned char device_regs[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};

ssize_t dev_read(void *filp, unsigned char __user *buf, size_t count) {
    (void)filp;
    size_t n = count < sizeof(device_regs) ? count : sizeof(device_regs);
    if (copy_to_user_emu(buf, device_regs, n))
        return -EFAULT;
    return (ssize_t)n;
}

ssize_t dev_write(void *filp, const unsigned char __user *buf, size_t count) {
    (void)filp;
    size_t n = count < sizeof(device_regs) ? count : sizeof(device_regs);
    if (copy_from_user_emu(device_regs, buf, n))
        return -EFAULT;
    return (ssize_t)n;
}
