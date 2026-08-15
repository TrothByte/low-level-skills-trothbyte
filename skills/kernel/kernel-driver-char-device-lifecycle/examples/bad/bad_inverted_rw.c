/*
 * BAD: // intentionally incorrect — inverted read/write direction.
 * dev_read() copies FROM user space INTO the device structure. read(2) means
 * the user is reading FROM the device, so the kernel must copy device-owned
 * data out via copy_to_user. This inversion leaks kernel memory content into
 * the user buffer and lets userspace write into device state. Compiles clean;
 * must be caught by review.
 */
#include "stubs.h"

static unsigned char device_regs[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};

ssize_t dev_read(void *filp, unsigned char __user *buf, size_t count) {
    (void)filp;
    size_t n = count < sizeof(device_regs) ? count : sizeof(device_regs);
    return copy_from_user_emu(device_regs, buf, n);
}
