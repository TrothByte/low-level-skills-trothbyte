# GOOD: canonical Kbuild two-file pattern for an out-of-tree module.
# The module Makefile only declares targets; the driver makefile (this file,
# named Makefile, or a wrapper) delegates the actual build to the kernel's
# Kbuild with `-C $(KDIR) M=$(PWD) modules`. KDIR points at the RUNNING
# kernel's build tree by default and can be overridden for a specific target.
obj-m := good_mod.o
good_mod-objs := core.o helper.o

ccflags-y += -Wall

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
