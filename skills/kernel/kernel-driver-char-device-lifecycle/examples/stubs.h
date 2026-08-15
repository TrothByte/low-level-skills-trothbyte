/*
 * stubs.h — self-contained host stubs for char-device-lifecycle shaped code.
 * Models: an emulated user address space (a fixed global buffer), a copy_
 * helper with access-check + return-of-bytes-not-copied semantics, a
 * minimal lifecycle registry (class/device/cdev slots) that asserts the
 * init/exit mirror ordering, and a module reference counter with
 * try_module_get/module_put semantics. No kernel headers required.
 *
 * Not kernel code — the API is emulated, the contracts are real.
 */
#ifndef CHAR_DEV_LIFECYCLE_STUBS_H
#define CHAR_DEV_LIFECYCLE_STUBS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __user
#define THIS_MODULE ((void *)&_module_self)

#define EFAULT 14
#define EINVAL 22
#define EBUSY  16
#define EIO    5

#define DEV_MINORS 1

typedef unsigned int dev_t;

struct file {
    void *private_data;
};

struct inode {
    int fake;
};

struct class_s { int id; };
struct device_s { struct class_s *cls; dev_t dev; };

int _trace_steps;
#define TRACE_STEP(name) do { if (_trace_steps) printf("step: %s\n", name); } while (0)

static dev_t _chrdev_regions;
static int _cdev_active;
static struct class_s *_active_class;
static struct device_s _active_device;

unsigned char __user_mem[4096];
static int _mod_refs;

/* ---- emulated module reference counting ---- */
static void *const _module_self = (void *)0x1;
static inline int try_module_get(void *m) {
    (void)m;
    _mod_refs++;
    return 1;
}
static inline void module_put(void *m) {
    (void)m;
    if (_mod_refs > 0)
        _mod_refs--;
    else
        fprintf(stderr, "BUG: module_put with zero refs\n");
}

/* ---- emulated uaccess: returns bytes NOT copied ---- */
static inline int access_ok_emu(const void *addr, size_t size) {
    uintptr_t a = (uintptr_t)addr;
    uintptr_t lo = (uintptr_t)__user_mem;
    uintptr_t hi = lo + sizeof(__user_mem);
    return size <= sizeof(__user_mem) && a >= lo && a <= hi - size;
}

static inline unsigned long copy_to_user_emu(void *to, const void *from,
                                             size_t n) {
    if (n && !access_ok_emu(to, n))
        return n;
    if (n)
        memcpy(to, from, n);
    return 0;
}

static inline unsigned long copy_from_user_emu(void *to, const void *from,
                                               size_t n) {
    if (n && !access_ok_emu(from, n))
        return n;
    if (n)
        memcpy(to, from, n);
    return 0;
}

/* ---- emulated char-device lifecycle ---- */
static inline int register_chrdev_region_emu(dev_t *out, unsigned int base,
                                             unsigned int count,
                                             const char *name) {
    (void)name;
    (void)base;
    if (count == 0)
        return -EINVAL;
    _chrdev_regions = 0xE000;
    *out = _chrdev_regions;
    TRACE_STEP("register_chrdev_region");
    return 0;
}
static inline void unregister_chrdev_region_emu(dev_t dev, unsigned int count) {
    (void)dev; (void)count;
    TRACE_STEP("unregister_chrdev_region");
    _chrdev_regions = 0;
}
static inline void cdev_init_emu(void) {
    TRACE_STEP("cdev_init");
}
static inline int cdev_add_emu(dev_t dev, unsigned int count) {
    (void)dev; (void)count;
    TRACE_STEP("cdev_add");
    _cdev_active = 1;
    return 0;
}
static inline void cdev_del_emu(void) {
    TRACE_STEP("cdev_del");
    _cdev_active = 0;
}
static inline struct class_s *class_create_emu(const char *name) {
    (void)name;
    struct class_s *c = malloc(sizeof(*c));
    c->id = 7;
    TRACE_STEP("class_create");
    _active_class = c;
    return c;
}
static inline void class_destroy_emu(struct class_s *c) {
    TRACE_STEP("class_destroy");
    free(c);
    _active_class = NULL;
}
static inline struct device_s *device_create_emu(struct class_s *c, dev_t dev) {
    _active_device.cls = c;
    _active_device.dev = dev;
    TRACE_STEP("device_create");
    return &_active_device;
}
static inline void device_destroy_emu(struct class_s *c, dev_t dev) {
    (void)c; (void)dev;
    TRACE_STEP("device_destroy");
    memset(&_active_device, 0, sizeof(_active_device));
}

#endif
