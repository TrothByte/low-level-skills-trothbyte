/*
 * BAD: // intentionally incorrect — no module reference count on open.
 * open() stores a pointer and returns 0 without try_module_get, so the module
 * reference count stays 0 and rmmod succeeds while the file is open; the next
 * read dereferences freed code/data. The stub asserts the count and prints the
 * violation.
 */
#include "stubs.h"

static int device_open(struct inode *inode, struct file *filp) {
    (void)inode;
    filp->private_data = (void *)0xDEADBEEF;
    return 0; /* no try_module_get */
}

static int device_release(struct inode *inode, struct file *filp) {
    (void)inode;
    (void)filp;
    return 0; /* no module_put */
}

int main(void) {
    struct inode in;
    struct file f;
    device_open(&in, &f);
    printf("refs after open (expect 0 if unprotected): %d\n", _mod_refs);
    device_release(&in, &f);
    return 0;
}
