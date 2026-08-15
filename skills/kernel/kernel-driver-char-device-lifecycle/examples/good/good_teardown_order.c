/*
 * GOOD: teardown is the exact reverse of init, class_destroy called once.
 * The stub's step trace asserts the ordering; every init error path unwinds
 * only what was already set up.
 */
#include "stubs.h"

static dev_t devno;
static struct class_s *cls;
static int devices_created;

int init_correct_order(void) {
    if (register_chrdev_region_emu(&devno, 0, DEV_MINORS, "demo") < 0)
        return -EIO;
    cdev_init_emu();
    if (cdev_add_emu(devno, DEV_MINORS) < 0) {
        unregister_chrdev_region_emu(devno, DEV_MINORS);
        return -EIO;
    }
    cls = class_create_emu("demo");
    if (!cls) {
        cdev_del_emu();
        unregister_chrdev_region_emu(devno, DEV_MINORS);
        return -ENOMEM;
    }
    device_create_emu(cls, devno);
    devices_created = 1;
    return 0;
}

void exit_correct_order(void) {
    if (devices_created) {
        device_destroy_emu(cls, devno);
        devices_created = 0;
    }
    class_destroy_emu(cls);
    cdev_del_emu();
    unregister_chrdev_region_emu(devno, DEV_MINORS);
}
