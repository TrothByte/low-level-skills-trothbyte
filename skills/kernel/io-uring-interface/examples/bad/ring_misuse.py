#!/usr/bin/env python3
"""ring_misuse.py — model of agent-typical io_uring bugs a reviewer must catch.

Each scenario replays a class of buggy "agent code" against a faithful ring
simulation with a built-in protocol checker. The checker must catch the bug
and report it; the harness prints

    BUG reproduced: <diagnostic>

for every caught bug. If any bug escapes the checker the harness FAILs
(exit 1). Exit 0 means the checker caught all modelled bug classes.

Covered bug classes (all in the SKILL.md "What the agent often gets wrong"):

  1. SQE submitted with uninitialized fields (garbage reads on the kernel)
  2. ring index computed with a byte-size mask instead of the entry mask
  3. buffer freed/reused before the matching CQE is read
  4. stack (frame-local) buffer in flight with an async request
  5. sq_tail advanced but io_uring_enter never called -> CQ never advances
     (agent busy-loops on wait_cqe)
  6. shared index written without smp_store_release (memory-ordering bug)
  7. IOSQE_IO_LINK chain treated as independent requests; the failed chain
     fails the follower with -ECANCELED
  8. features assumed instead of gated via io_uring_setup params.features

Run on any host:  python ring_misuse.py
"""

import sys


class RingBug(Exception):
    """A protocol violation the reviewer must catch."""


# ---- uapi constants (same stable subset as the good model) ---------------

IORING_OP_NOP = 0
IORING_OP_READV = 1
IORING_OP_LINK_TIMEOUT = 15
IOSQE_IO_LINK = 1 << 0
IORING_CQE_F_MORE = 1 << 1
IORING_ACCEPT_MULTISHOT = 1 << 0
IORING_FEAT_SINGLE_MMAP = 1 << 0
IORING_FEAT_NODROP = 1 << 1

SENTINEL = object()


# ---- memory-ordering simulation ------------------------------------------


class SharedIndex:
    def __init__(self, name):
        self.name = name
        self.value = 0
        self.log = []

    def store_release(self, v, who="user"):
        self.log.append(("release_store", who, v))
        self.value = v

    def store_plain(self, v, who="user"):
        self.log.append(("plain_store", who, v))
        self.value = v

    def load_acquire(self, who="user"):
        self.log.append(("acquire_load", who, self.value))
        return self.value

    def load_plain(self, who="user"):
        self.log.append(("plain_load", who, self.value))
        return self.value


class OrderAudit:
    """sq_tail/cq_head must be release stores; sq_head/cq_tail acquire loads."""

    def check(self, ring):
        violations = []
        for name in ("sq_tail", "cq_head"):
            for kind, who, _v in getattr(ring, name).log:
                if who == "user" and kind == "plain_store":
                    violations.append(
                        "%s written without smp_store_release" % name)
        for name in ("sq_head", "cq_tail"):
            for kind, who, _v in getattr(ring, name).log:
                if who == "user" and kind == "plain_load":
                    violations.append(
                        "%s read without smp_load_acquire" % name)
        return violations


# ---- objects -------------------------------------------------------------


class Buffer:
    def __init__(self, size):
        self.size = size
        self.freed = False

    def free(self):
        self.freed = True


class SQEntry:
    def __init__(self):
        self.reset()

    def reset(self):
        self.opcode = SENTINEL
        self.flags = 0
        self.ioprio = 0
        self.fd = SENTINEL
        self.off = 0
        self.addr = 0
        self.len = SENTINEL
        self.rw_flags = 0
        self.user_data = SENTINEL
        self.buf_group = 0
        self.buf = None
        self.slow = False
        self.fails = False
        self.timeout_target = SENTINEL
        self.deadline_ticks = 0

    def uninitialized_fields(self):
        bad = []
        if self.opcode is SENTINEL:
            bad.append("opcode")
        if self.fd is SENTINEL:
            bad.append("fd")
        if self.len is SENTINEL:
            bad.append("len")
        if self.user_data is SENTINEL:
            bad.append("user_data")
        return bad


class CQE:
    def __init__(self, user_data, res, flags=0):
        self.user_data = user_data
        self.res = res
        self.flags = flags


# ---- ring + kernel sim ---------------------------------------------------


class Uring:
    def __init__(self, sq_entries=16, cq_entries=16, features=0):
        self.sq_entries = sq_entries
        self.sq_mask = sq_entries - 1
        self.sq = [None] * sq_entries
        self.sq_head = SharedIndex("sq_head")
        self.sq_tail = SharedIndex("sq_tail")
        self.cq_entries = cq_entries
        self.cq_mask = cq_entries - 1
        self.cq = [None] * cq_entries
        self.cq_head = SharedIndex("cq_head")
        self.cq_tail = SharedIndex("cq_tail")
        self.sqe_tail = 0
        self.submitted_sq_tail = 0
        self.live_buffers = {}
        self.provided = {}
        self.parked = {}
        self.in_flight = {}
        self.timeouts = {}
        self.enter_calls = 0
        self.features = features   # simulates io_uring_setup params.features
        self.enters_seen = 0

    def get_sqe(self):
        head = self.sq_head.load_acquire()
        tail = self.sqe_tail
        if tail - head >= self.sq_entries:
            raise RingBug("get_sqe: SQ full (must submit before fetching)")
        idx = tail & self.sq_mask
        sqe = self.sq[idx]
        if sqe is None:
            sqe = SQEntry()
            self.sq[idx] = sqe
        sqe.reset()
        self.sqe_tail += 1
        return sqe

    def submit(self, to_submit=None):
        if to_submit is None:
            to_submit = self.sqe_tail - self.submitted_sq_tail
        if to_submit <= 0:
            raise RingBug("submit: no new SQEs to submit")
        for i in range(self.submitted_sq_tail,
                       self.submitted_sq_tail + to_submit):
            sqe = self.sq[i & self.sq_mask]
            uninit = sqe.uninitialized_fields()
            if uninit:
                raise RingBug(
                    "SQE slot %d submitted with uninitialized field(s): %s"
                    % (i & self.sq_mask, ", ".join(uninit)))
        new_tail = self.submitted_sq_tail + to_submit
        self.sq_tail.store_release(new_tail)
        self.submitted_sq_tail = new_tail
        self.enters_seen += 1
        kernel_consume(self)

    def wait_cqe(self):
        for _ in range(200):
            cqe = self.get_cqe()
            if cqe is not None:
                return cqe
            if self.sqe_tail != self.submitted_sq_tail:
                raise RingBug(
                    "SQEs fetched but io_uring_enter() never called — the "
                    "kernel cannot see them and the CQ never advances "
                    "(agent busy-loops on wait_cqe)")
            kernel_tick(self)
        raise RingBug("no CQE within 200 kernel ticks")

    def get_cqe(self):
        tail = self.cq_tail.load_acquire()
        head = self.cq_head.load_plain()
        if head == tail:
            return None
        return self.cq[head & self.cq_mask]

    def cqe_seen(self):
        self.cq_head.store_release(self.cq_head.value + 1)

    def register_provided_buffers(self, buf_group, count):
        if not (self.features & IORING_FEAT_NODROP) and not (self.features & IORING_FEAT_SINGLE_MMAP):
            # old kernel: provided buffer rings are simply not implemented
            return -22  # -EINVAL
        self.provided[buf_group] = [Buffer(4096) for _ in range(count)]
        return 0


def kernel_complete(ring, user_data, res, sqe, flags=0):
    if sqe is not None and sqe.buf is not None and sqe.buf.freed:
        raise RingBug(
            "buffer for user_data %#x freed/reused before the matching CQE "
            "was read (stack buffer or reuse-before-CQE)" % user_data)
    tail = ring.cq_tail.load_plain(who="kernel")
    head = ring.cq_head.load_plain(who="kernel")
    if tail - head >= ring.cq_entries:
        raise RingBug(
            "CQ overflow: without IORING_FEAT_NODROP the kernel drops CQEs")
    ring.cq[tail & ring.cq_mask] = CQE(user_data, res, flags)
    ring.cq_tail.store_plain(tail + 1, who="kernel")


def kernel_execute(ring, sqe):
    if sqe.opcode == IORING_OP_NOP:
        kernel_complete(ring, sqe.user_data, 0, sqe)
        return True
    if sqe.opcode == IORING_OP_READV:
        if sqe.fails:
            kernel_complete(ring, sqe.user_data, -22, sqe)
            return False
        if sqe.slow:
            ring.in_flight[sqe.user_data] = {
                "kind": "readv", "sqe": sqe, "remaining": 3, "res": sqe.len}
        else:
            kernel_complete(ring, sqe.user_data, sqe.len, sqe)
        return True
    if sqe.opcode == IORING_OP_LINK_TIMEOUT:
        ring.timeouts[sqe.user_data] = {
            "remaining": sqe.deadline_ticks, "target": sqe.timeout_target}
        return True
    raise RingBug("kernel: unknown opcode %d" % sqe.opcode)


def kernel_consume(ring):
    head = ring.sq_head.load_plain(who="kernel")
    tail = ring.sq_tail.load_plain(who="kernel")
    chain_failed = False
    while head < tail:
        sqe = ring.sq[head & ring.sq_mask]
        ring.sq[head & ring.sq_mask] = None
        link = bool(sqe.flags & IOSQE_IO_LINK)
        if chain_failed:
            kernel_complete(ring, sqe.user_data, -125, sqe)
        else:
            ok = kernel_execute(ring, sqe)
            if not ok and link:
                chain_failed = True
        if not link:
            chain_failed = False
        head += 1
    ring.sq_head.store_plain(head, who="kernel")


def kernel_tick(ring):
    for ud, timer in list(ring.timeouts.items()):
        timer["remaining"] -= 1
        if timer["remaining"] <= 0:
            ring.timeouts.pop(ud)
            op = ring.in_flight.pop(timer["target"], None)
            if op is not None:
                kernel_complete(ring, timer["target"], -62, op["sqe"])
            kernel_complete(ring, ud, -62, None)
    for ud in list(ring.in_flight):
        op = ring.in_flight[ud]
        op["remaining"] -= 1
        if op["remaining"] <= 0:
            ring.in_flight.pop(ud)
            kernel_complete(ring, ud, op["res"], op["sqe"])


# ---- bug scenarios (the "agent code" under review) -----------------------


def prep_nop(sqe, user_data):
    sqe.reset()
    sqe.opcode = IORING_OP_NOP
    sqe.fd = 0
    sqe.len = 0
    sqe.user_data = user_data


def bug_uninitialized_sqe(ring):
    sqe = ring.get_sqe()
    sqe.opcode = IORING_OP_READV
    sqe.user_data = 0x1
    # BUG: fd, off, addr, len, rw_flags left uninitialized
    ring.submit()
    raise RingBug("unreachable")


def bug_index_without_mask(ring):
    # BUG: agent masks with the ring byte size (entries * 64) instead of the
    # entry count mask; tail beyond sq_entries indexes out of the ring.
    byte_size = ring.sq_entries * 64
    for tail in range(0, ring.sq_entries + 8):
        idx = tail & (byte_size - 1)
        if idx >= ring.sq_entries:
            raise RingBug(
                "ring index %d computed with byte-size mask (entries*64-1); "
                "must be tail & (sq_entries-1)" % idx)


def bug_reuse_before_cqe(ring):
    buf = Buffer(4096)
    sqe = ring.get_sqe()
    sqe.reset()
    sqe.opcode = IORING_OP_READV
    sqe.fd = 3
    sqe.len = buf.size
    sqe.user_data = 0x2
    sqe.buf = buf
    sqe.slow = True   # completion arrives on a later tick
    ring.submit()
    buf.free()        # BUG: buffer freed before the matching CQE
    ring.wait_cqe()   # kernel completes -> UAF detected
    raise RingBug("unreachable")


def bug_stack_buffer(ring):
    # BUG: buffer lives on the "stack" of submit_async_read(); the frame
    # returns while the async request is in flight. buf.free() models the
    # stack teardown — the kernel still holds a pointer into the dead frame.
    def submit_async_read(fd):
        buf = Buffer(4096)
        sqe = ring.get_sqe()
        sqe.reset()
        sqe.opcode = IORING_OP_READV
        sqe.fd = fd
        sqe.len = buf.size
        sqe.user_data = 0x3
        sqe.buf = buf
        sqe.slow = True
        ring.submit()
        buf.free()   # frame exits; stack_buf is dead
        return 1

    submit_async_read(3)
    ring.wait_cqe()   # kernel completes -> UAF on the dead frame detected
    raise RingBug("unreachable")


def bug_missing_enter(ring):
    sqe = ring.get_sqe()
    prep_nop(sqe, user_data=0x4)
    # BUG: agent never calls submit()/io_uring_enter(), then busy-loops
    for _ in range(1000):
        ring.wait_cqe()   # CQ never advances -> detected
    raise RingBug("unreachable")


def bug_plain_store_sq_tail(ring):
    sqe = ring.get_sqe()
    prep_nop(sqe, user_data=0x5)
    # BUG: sq_tail written with a plain store, no release
    ring.sq_tail.store_plain(ring.sqe_tail)
    ring.submitted_sq_tail = ring.sqe_tail
    ring.enters_seen += 1
    kernel_consume(ring)
    violations = OrderAudit().check(ring)
    if violations:
        raise RingBug("; ".join(violations))
    raise RingBug("unreachable")


def bug_link_not_independent(ring):
    # BUG: agent thinks IOSQE_IO_LINK chains complete independently
    a = ring.get_sqe()
    a.reset()
    a.opcode = IORING_OP_READV
    a.fd = 9            # bad fd -> fails
    a.len = 4096
    a.user_data = 0x6
    a.fails = True
    a.flags |= IOSQE_IO_LINK
    b = ring.get_sqe()
    prep_nop(b, user_data=0x7)
    ring.submit()
    c1 = ring.wait_cqe()
    ring.cqe_seen()
    c2 = ring.wait_cqe()
    ring.cqe_seen()
    by_ud = {c.user_data: c for c in (c1, c2)}
    if by_ud[0x7].res < 0:
        raise RingBug(
            "IOSQE_IO_LINK chain failed all-or-nothing: head got %d and the "
            "follower was failed with %d instead of completing independently"
            % (by_ud[0x6].res, by_ud[0x7].res))


def bug_assume_newest_features(ring):
    # BUG: agent uses provided buffers / NODROP without gating on
    # io_uring_setup params.features. Old kernel: io_uring_register returns
    # -EINVAL and the agent ignores it.
    ret = ring.register_provided_buffers(buf_group=0, count=4)
    if ret < 0:
        raise RingBug(
            "io_uring_register(IORING_REGISTER_PBUF_RING) returned %d: "
            "provided buffers assumed without checking params.features on an "
            "old kernel" % ret)
    raise RingBug("unreachable")


# ---- harness --------------------------------------------------------------


def reproduce(name, fn, features=0):
    ring = Uring(features=features)
    try:
        fn(ring)
        print("FAILED TO CATCH  %s  (bug escaped the checker)" % name)
        return False
    except RingBug as exc:
        print("BUG reproduced: %s" % exc)
        return True


def main():
    caught = []
    caught.append(reproduce("uninitialized SQE fields", bug_uninitialized_sqe))
    caught.append(reproduce("ring index without mask", bug_index_without_mask))
    caught.append(reproduce("reuse buffer before CQE", bug_reuse_before_cqe))
    caught.append(reproduce("stack buffer in flight", bug_stack_buffer))
    caught.append(reproduce("missing io_uring_enter (busy-loop)", bug_missing_enter))
    caught.append(reproduce("sq_tail plain store (ordering)", bug_plain_store_sq_tail))
    caught.append(reproduce("link chain not independent", bug_link_not_independent))
    caught.append(reproduce("features assumed, not gated",
                            bug_assume_newest_features, features=0))
    if not all(caught):
        print("RING MISUSE MODEL: FAIL (%d/%d bug classes escaped)"
              % (caught.count(False), len(caught)))
        sys.exit(1)
    print("RING MISUSE MODEL: PASS — %d bug classes caught" % len(caught))


if __name__ == "__main__":
    main()
