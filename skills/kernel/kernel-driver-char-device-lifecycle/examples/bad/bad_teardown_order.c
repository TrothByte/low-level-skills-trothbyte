/*
 * BAD: // intentionally incorrect — double class_destroy + wrong teardown order.
 * module_exit destroys the class before the device, and an error path in init
 * also calls class_destroy. On unload the device still references the class;
 * the second class_destroy double-frees. The stub counts frees and prints the
 * failure.
 */
#include "stubs.h"

static dev_t devno;
static struct class_s *cls;

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
    return 0;
}

void exit_wrong_order(void) {
    class_destroy_emu(cls);            /* device still references cls */
    device_destroy_emu(cls, devno);
    cdev_del_emu();
    unregister_chrdev_region_emu(devno, DEV_MINORS);
}
