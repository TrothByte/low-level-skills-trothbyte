#!/usr/bin/env python3
"""Host-run model of the atomic-context allocation mistake in a Rust
module.

In the kernel, allocating with a sleepable GFP flag (GFP_KERNEL) from an
interrupt handler or while a spinlock is held sleeps with preemption
disabled — the classic "BUG: scheduling while atomic" oops. The correct flag
is GFP_ATOMIC, and the kernel crate's allocation API is fallible and
flag-aware precisely so this context is explicit.

The second function models exception/panic-style error handling instead of
errno Result propagation: a panic in module code is a kernel oops, not a
recoverable error.

Run:  python examples/bad/alloc_context_misuse.py
Expected: two FAIL lines, exit 1.
"""

import sys


class GfpFlags:
    def __init__(self, name, atomic):
        self.name = name
        self.atomic = atomic


GFP_KERNEL = GfpFlags("GFP_KERNEL", atomic=False)
GFP_ATOMIC = GfpFlags("GFP_ATOMIC", atomic=True)


class AtomicContext:
    def __init__(self):
        self.in_irq = True
        self.spinlock_held = True


def alloc_in_interrupt_handler():
    ctx = AtomicContext()
    # WRONG: sleepable allocation flag inside an IRQ handler while a spinlock
    # is held — would sleep with preemption disabled (kernel oops).
    gfp = GFP_KERNEL
    if ctx.in_irq and ctx.spinlock_held and not gfp.atomic:
        print("FAIL: GFP_KERNEL allocation in interrupt context")
        print("      (kernel: 'BUG: scheduling while atomic')")
        return 1
    return 0


def panic_style_error():
    # WRONG: exception/panic handling instead of errno Result propagation.
    try:
        return 1 // 0
    except ZeroDivisionError as exc:
        print(f"FAIL: panic-style abort instead of Error propagation ({exc})")
        return 1


if __name__ == "__main__":
    rc = alloc_in_interrupt_handler()
    rc += panic_style_error()
    print("host model complete: a real kernel build would reject both")
    sys.exit(1 if rc else 0)
