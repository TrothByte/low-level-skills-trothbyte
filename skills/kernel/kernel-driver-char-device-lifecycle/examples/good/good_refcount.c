/*
 * GOOD: module reference counting protects unload while open.
 * try_module_get in open, module_put in release; rmmod is refused while any
 * open file holds a reference. The stub prints the ref count: 1 while open,
 * 0 after close.
 */
#include "stubs.h"

static int device_open(struct inode *inode, struct file *filp) {
    (void)inode;
    if (!try_module_get(THIS_MODULE))
        return -EBUSY;
    filp->private_data = (void *)0xDEADBEEF;
    return 0;
}

static int device_release(struct inode *inode, struct file *filp) {
    (void)inode;
    (void)filp;
    module_put(THIS_MODULE);
    return 0;
}

int main(void) {
    struct inode in;
    struct file f;
    if (device_open(&in, &f) == 0) {
        printf("refs while open: %d (rmmod must refuse: -EBUSY)\n", _mod_refs);
        device_release(&in, &f);
        printf("refs after close: %d (rmmod allowed)\n", _mod_refs);
    }
    return 0;
}
