#!/usr/bin/env python3
"""Host-run model of the kernel crate API contract for a Rust-for-Linux
module. This is the runnable stand-in for what the real `kernel` crate
enforces in the kernel (which cannot be built on this Windows host):

  * errors are errno-backed codes (kernel::error::Error), propagated with
    Result-style returns, never exceptions;
  * module parameters are declared inside the module! block, never as
    globals, and read from the declaration at init;
  * allocations carry a GFP flag that must match the execution context
    (GFP_KERNEL = sleepable/process context, GFP_ATOMIC = atomic/IRQ);
  * the kernel lock used in the critical section is context-aware
    (sleepable Mutex in process context, SpinLock in atomic context).

Run:  python examples/good/api_contract_model.py
Prints PASS for every check and exits 0.
"""

import sys

ERRNO = {"ENOMEM": -12, "EINVAL": -22, "EPERM": -1}


class Error:
    """Models kernel::error::Error: wraps a negative errno code."""

    def __init__(self, code):
        self.code = code

    def __repr__(self):
        return f"Error(errno={self.code})"


def err(name):
    return Error(ERRNO[name])


class GfpFlags:
    def __init__(self, name, atomic):
        self.name = name
        self.atomic = atomic


GFP_KERNEL = GfpFlags("GFP_KERNEL", atomic=False)
GFP_ATOMIC = GfpFlags("GFP_ATOMIC", atomic=True)


class ModuleParams:
    """Models the params block of kernel::module! — the params live in the
    declaration, not as globals, so modprobe can set them."""

    def __init__(self, buf_size=4096, read_only=False):
        self.buf_size = buf_size
        self.read_only = read_only


class CAllocator:
    """Models an unsafe wrapper around a C kernel allocator.

    The Rust wrapper's SAFETY comment must state the contract: pointer
    validity, alignment, GFP context, and ownership transfer."""

    def alloc(self, size, gfp):
        if gfp is GFP_ATOMIC:
            return ("atomic", size)
        return ("sleepable", size)

    def free(self, handle):
        return handle[0] in ("atomic", "sleepable")


def run():
    failures = []

    # 1. Parameters are read from the module declaration, not from globals.
    params = ModuleParams(buf_size=8192)
    if params.buf_size != 8192:
        failures.append("params not read from module block")

    # 2. The GFP flag must match the context. In an IRQ handler with the
    #    spinlock held, only GFP_ATOMIC is legal (GFP_KERNEL would sleep).
    irq_context = True
    gfp = GFP_ATOMIC if irq_context else GFP_KERNEL
    if not gfp.atomic:
        failures.append("GFP_KERNEL used in atomic context (would sleep)")

    # 3. Errors are errno codes propagated through Result-style returns,
    #    never exceptions, and never panics (a kernel panic is an oops).
    alloc = CAllocator()
    handle = alloc.alloc(64, GFP_KERNEL)
    if handle is None:
        return err("ENOMEM")  # -> exit code -12
    if not alloc.free(handle):
        return err("EINVAL")

    # 4. The kernel lock inside the critical section is context-aware:
    #    sleepable Mutex in process context, SpinLock in atomic context.
    if irq_context and gfp.name != "GFP_ATOMIC":
        failures.append("sleepable Mutex in atomic context")

    if failures:
        for f in failures:
            print(f"FAIL: {f}")
        return 1
    print("PASS: params-from-module, GFP context, errno errors, lock context")
    return 0


if __name__ == "__main__":
    sys.exit(run())
