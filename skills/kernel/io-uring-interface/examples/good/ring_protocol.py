#!/usr/bin/env python3
"""ring_protocol.py — host-runnable model of the io_uring ring protocol.

Faithful model of the lock-free SQ/CQ shared-memory rings between a userspace
thread and the kernel:

  - SQE fetch and fill discipline (get_sqe -> prep_* -> submit)
  - ring index math:  idx = tail & mask  (rings are power-of-two sized)
  - memory ordering:  sq_tail/cq_head written with smp_store_release,
                      cq_tail/sq_head read with smp_load_acquire
  - io_uring_enter(to_submit, to_wait) submits and/or waits
  - CQE consumption:  cq_tail acquire load, process CQE, cq_head release store
  - CQE res < 0 encodes -errno
  - linked requests (IOSQE_IO_LINK): all-or-nothing; a failed link chain is
    failed with -ECANCELED, and an IORING_OP_LINK_TIMEOUT that fires fails the
    head request with -ETIME
  - provided buffer rings + multishot accept: completions keep coming while
    buffers remain; the request re-arms when the buffer ring is replenished
  - feature gating via params.features (IORING_FEAT_*)

Run on any host:  python ring_protocol.py
Prints PASS/FAIL per scenario. Exit 0 iff all scenarios pass.

Opcode/feature constants below are the uapi/linux/io_uring.h values that are
stable across kernel 5.1+; opcodes added later are out of the model's scope.
"""

import sys


class RingError(Exception):
    """Protocol violation (bad usage of the ring)."""


# ---------------------------------------------------------------------------
# uapi constants (stable subset, kernel 5.1+)
# ---------------------------------------------------------------------------

# io_uring_op
IORING_OP_NOP = 0
IORING_OP_READV = 1
IORING_OP_WRITEV = 2
IORING_OP_TIMEOUT = 11
IORING_OP_ACCEPT = 13
IORING_OP_LINK_TIMEOUT = 15

# sqe->flags
IOSQE_IO_LINK = 1 << 0

# io_uring_enter flags
IORING_ENTER_GETEVENTS = 1 << 0

# cqe->flags
IORING_CQE_F_MORE = 1 << 1

# accept flags (io_uring_accept)
IORING_ACCEPT_MULTISHOT = 1 << 0

# io_uring_setup params.features bits
IORING_FEAT_SINGLE_MMAP = 1 << 0
IORING_FEAT_NODROP = 1 << 1
IORING_FEAT_SUBMIT_STABLE = 1 << 2

# errno values reported as res = -errno in CQEs
_ERRNO_NAMES = {
    11: "EAGAIN",
    14: "EFAULT",
    22: "EINVAL",
    62: "ETIME",
    125: "ECANCELED",
}

SENTINEL = object()


def errno_of(res):
    """res < 0 is -errno. Returns the errno name, or None if res >= 0."""
    if res < 0:
        return _ERRNO_NAMES.get(-res, "errno %d" % -res)
    return None


# ---------------------------------------------------------------------------
# memory-ordering simulation (protocol rule: lock-free shared indices)
# ---------------------------------------------------------------------------


class SharedIndex:
    """A head/tail index shared between userspace and the kernel.

    Logs every access so an OrderAudit can verify that the userspace side used
    the required ordering primitive:
      - stores to sq_tail / cq_head  must be release stores
      - loads of cq_tail / sq_head   must be acquire loads
    who="kernel" accesses are the kernel side (WRITE_ONCE in the real kernel)
    and are excluded from the user-side audit.
    """

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
    """Checks the C-level ordering contract on the shared indices:

        sq_tail   user stores must be smp_store_release
        cq_head   user stores must be smp_store_release
        sq_head   user loads must be smp_load_acquire
        cq_tail   user loads must be smp_load_acquire

    A plain (unordered) user access to any shared index is a violation.
    """

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


# ---------------------------------------------------------------------------
# SQE / CQE / buffer objects
# ---------------------------------------------------------------------------


class Buffer:
    """A userspace I/O buffer. freed=True models the buffer being released
    (free()/reuse/stack frame exit) while a request is still in flight."""

    def __init__(self, size):
        self.size = size
        self.freed = False

    def free(self):
        self.freed = True


class SQEntry:
    """A 64-byte submission queue entry (modelled fields)."""

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
        self.buf = None          # model-only: buffer object for readv
        self.slow = False        # model-only: readv completes after ticks
        self.timeout_target = SENTINEL   # model-only: LINK_TIMEOUT target ud
        self.deadline_ticks = 0  # model-only: LINK_TIMEOUT deadline

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
    """16-byte completion queue entry: user_data, res, flags."""

    def __init__(self, user_data, res, flags=0):
        self.user_data = user_data
        self.res = res          # >= 0 result, or -errno
        self.flags = flags


# ---------------------------------------------------------------------------
# liburing-style preparation helpers (they fully initialize the SQE)
# ---------------------------------------------------------------------------


def prep_nop(sqe, user_data):
    sqe.reset()
    sqe.opcode = IORING_OP_NOP
    sqe.fd = 0
    sqe.len = 0
    sqe.user_data = user_data


def prep_readv(sqe, fd, buf, user_data, off=0, slow=False, link=False):
    sqe.reset()
    sqe.opcode = IORING_OP_READV
    sqe.fd = fd
    sqe.off = off
    sqe.len = buf.size
    sqe.rw_flags = 0
    sqe.user_data = user_data
    sqe.buf = buf
    sqe.slow = slow
    if link:
        sqe.flags |= IOSQE_IO_LINK


def prep_link_timeout(sqe, deadline_ticks, user_data, target):
    sqe.reset()
    sqe.opcode = IORING_OP_LINK_TIMEOUT
    sqe.fd = 0
    sqe.len = 0
    sqe.user_data = user_data
    sqe.timeout_target = target
    sqe.deadline_ticks = deadline_ticks


def prep_accept_multishot(sqe, fd, user_data, buf_group):
    sqe.reset()
    sqe.opcode = IORING_OP_ACCEPT
    sqe.fd = fd
    sqe.len = 0
    sqe.user_data = user_data
    sqe.buf_group = buf_group
    sqe.flags = 0
    sqe.ioprio = IORING_ACCEPT_MULTISHOT


# ---------------------------------------------------------------------------
# the ring (userspace side)
# ---------------------------------------------------------------------------


class Uring:
    def __init__(self, sq_entries=16, cq_entries=16):
        if sq_entries & (sq_entries - 1):
            raise RingError("sq_entries must be a power of two")
        if cq_entries & (cq_entries - 1):
            raise RingError("cq_entries must be a power of two")
        self.sq_entries = sq_entries
        self.sq_mask = sq_entries - 1
        self.sq = [None] * sq_entries
        self.sq_head = SharedIndex("sq_head")   # kernel writes, user reads
        self.sq_tail = SharedIndex("sq_tail")   # user writes, kernel reads
        self.cq_entries = cq_entries
        self.cq_mask = cq_entries - 1
        self.cq = [None] * cq_entries
        self.cq_head = SharedIndex("cq_head")   # user writes, kernel reads
        self.cq_tail = SharedIndex("cq_tail")   # kernel writes, user reads
        self.sqe_tail = 0            # user's private SQ tail (per-fetch copy)
        self.submitted_sq_tail = 0   # sq_tail value at last io_uring_enter
        self.live_buffers = {}       # user_data -> Buffer in flight
        self.provided = {}           # buf_group -> list of Buffers
        self.parked = {}             # ud -> sqe (multishot parked, no buffers)
        self.in_flight = {}          # ud -> {kind, sqe, remaining, res}
        self.timeouts = {}           # ud -> {remaining, target}

    # ---- protocol rule 1: fetch the next free SQE ------------------------
    def get_sqe(self):
        """Advance the user's private sq_tail by one and return the SQE at
        index (tail & mask). The shared sq_tail is only published (release
        store) at submit time — matching liburing's get_sqe/submit split."""
        head = self.sq_head.load_acquire()
        tail = self.sqe_tail
        if tail - head >= self.sq_entries:
            return None  # SQ full: must submit (or wait) before fetching more
        idx = tail & self.sq_mask
        sqe = self.sq[idx]
        if sqe is None:
            sqe = SQEntry()
            self.sq[idx] = sqe
        sqe.reset()
        self.sqe_tail += 1
        return sqe

    # ---- protocol rule 1/3: commit with a release store, then enter ------
    def submit(self, to_submit=None, to_wait=0, flags=0):
        if to_submit is None:
            to_submit = self.sqe_tail - self.submitted_sq_tail
        if to_submit <= 0:
            raise RingError("submit: no new SQEs to submit")
        for i in range(self.submitted_sq_tail,
                       self.submitted_sq_tail + to_submit):
            sqe = self.sq[i & self.sq_mask]
            if sqe is None:
                raise RingError(
                    "submit: slot %d has no SQE" % (i & self.sq_mask))
            uninit = sqe.uninitialized_fields()
            if uninit:
                raise RingError(
                    "submit: SQE slot %d has uninitialized field(s): %s"
                    % (i & self.sq_mask, ", ".join(uninit)))
        new_tail = self.submitted_sq_tail + to_submit
        # the shared sq_tail must be updated with a release store so the
        # kernel sees every initialized SQE before it sees the new tail
        self.sq_tail.store_release(new_tail)
        self.submitted_sq_tail = new_tail
        self.enter(to_submit, to_wait, flags)

    # ---- io_uring_enter ----------------------------------------------------
    def enter(self, to_submit, to_wait, flags=0):
        if to_submit > 0:
            kernel_consume(self)
        # in the real syscall, IORING_ENTER_GETEVENTS blocks here until at
        # least to_wait completions exist; the model resolves waits lazily in
        # wait_cqe() below.

    # ---- protocol rule 2: read CQEs, commit cq_head with release ---------
    def get_cqe(self):
        """Acquire-load cq_tail; if a completion is pending return it without
        committing cq_head."""
        tail = self.cq_tail.load_acquire()
        head = self.cq_head.load_plain()
        if head == tail:
            return None
        cqe = self.cq[head & self.cq_mask]
        if cqe is None:
            raise RingError("CQ slot %d empty at head %d" % (head & self.cq_mask, head))
        return cqe

    def wait_cqe(self):
        """Wait for the next completion (models uring_wait + kernel progress)."""
        for _ in range(200):
            cqe = self.get_cqe()
            if cqe is not None:
                return cqe
            kernel_tick(self)
        raise RingError("no CQE within 200 kernel ticks")

    def cqe_seen(self):
        """Re-commit cq_head with a release store after processing CQEs."""
        self.cq_head.store_release(self.cq_head.value + 1)

    # ---- provided buffer ring helpers (IORING_REGISTER_PBUF_RING) ---------
    def register_provided_buffers(self, buf_group, count):
        self.provided[buf_group] = [Buffer(4096) for _ in range(count)]

    def add_provided_buffers(self, buf_group, count):
        """io_uring_update_buf_ring: replenish buffers; re-arms parked
        multishot requests."""
        self.provided.setdefault(buf_group, [])
        for _ in range(count):
            self.provided[buf_group].append(Buffer(4096))
        kernel_resume_parked(self, buf_group)

    # ---- feature gate (protocol rule 8) -----------------------------------
    def params_features(self):
        return 0  # model default: oldest feature set (kernel 5.1 semantics)


# ---------------------------------------------------------------------------
# kernel side simulation
# ---------------------------------------------------------------------------


def kernel_complete(ring, user_data, res, sqe, flags=0):
    """Append a CQE. The kernel publishes it before advancing cq_tail."""
    if sqe is not None and sqe.buf is not None and sqe.buf.freed:
        raise RingError(
            "UAF: buffer for user_data %#x was freed/reused before the "
            "matching CQE was read" % user_data)
    tail = ring.cq_tail.load_plain(who="kernel")
    head = ring.cq_head.load_plain(who="kernel")
    if tail - head >= ring.cq_entries:
        raise RingError(
            "CQ overflow: kernel would drop the CQE (old kernels without "
            "IORING_FEAT_NODROP do exactly this)")
    ring.cq[tail & ring.cq_mask] = CQE(user_data, res, flags)
    ring.cq_tail.store_plain(tail + 1, who="kernel")


def kernel_execute(ring, sqe):
    """Return True if the SQE was accepted, False if it failed (a failed
    linked SQE fails the rest of its chain)."""
    if sqe.opcode == IORING_OP_NOP:
        kernel_complete(ring, sqe.user_data, 0, sqe)
        return True
    if sqe.opcode == IORING_OP_READV:
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
    if sqe.opcode == IORING_OP_ACCEPT:
        multishot = bool(sqe.ioprio & IORING_ACCEPT_MULTISHOT)
        if not multishot:
            kernel_complete(ring, sqe.user_data, 16, sqe)
            return True
        # multishot: keep producing completions while buffers remain
        grp = sqe.buf_group
        bufs = ring.provided.get(grp, [])
        while bufs:
            bufs.pop(0)
            kernel_complete(ring, sqe.user_data, 16, sqe,
                            flags=IORING_CQE_F_MORE)
        # buffer ring empty: the request parks and re-arms when buffers come
        ring.parked[sqe.user_data] = sqe
        return True
    raise RingError("kernel: unknown opcode %d" % sqe.opcode)


def kernel_consume(ring):
    """Kernel side of io_uring_enter: drain SQEs [sq_head, sq_tail)."""
    head = ring.sq_head.load_plain(who="kernel")
    tail = ring.sq_tail.load_plain(who="kernel")
    chain_failed = False
    while head < tail:
        sqe = ring.sq[head & ring.sq_mask]
        ring.sq[head & ring.sq_mask] = None
        if sqe is None:
            raise RingError("kernel: SQE slot %d empty" % (head & ring.sq_mask))
        link = bool(sqe.flags & IOSQE_IO_LINK)
        if chain_failed:
            # all-or-nothing: remaining members of the failed link chain are
            # failed with -ECANCELED without being executed
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
    """Advance simulated time: fire link timeouts, then finish async ops."""
    for ud, timer in list(ring.timeouts.items()):
        timer["remaining"] -= 1
        if timer["remaining"] <= 0:
            ring.timeouts.pop(ud)
            target = timer["target"]
            op = ring.in_flight.pop(target, None)
            if op is not None:
                # firing link timeout cancels the head request: -ETIME
                kernel_complete(ring, target, -62, op["sqe"])
            kernel_complete(ring, ud, -62, None)
    for ud in list(ring.in_flight):
        op = ring.in_flight[ud]
        op["remaining"] -= 1
        if op["remaining"] <= 0:
            ring.in_flight.pop(ud)
            kernel_complete(ring, ud, op["res"], op["sqe"])


def kernel_resume_parked(ring, buf_group):
    """A replenished provided-buffer ring re-arms parked multishot requests."""
    for ud, sqe in list(ring.parked.items()):
        if sqe.buf_group == buf_group and ring.provided.get(buf_group):
            ring.parked.pop(ud)
            kernel_execute(ring, sqe)


# ---------------------------------------------------------------------------
# scenarios
# ---------------------------------------------------------------------------


def scenario_basic_nop(ring):
    sqe = ring.get_sqe()
    prep_nop(sqe, user_data=0x10)
    ring.submit(to_submit=1, to_wait=1)
    cqe = ring.wait_cqe()
    ring.cqe_seen()
    assert cqe.user_data == 0x10, "nop user_data mismatch"
    assert cqe.res == 0, "nop res %d != 0" % cqe.res
    assert errno_of(cqe.res) is None, "res >= 0 must not decode as errno"
    # geometry: indices are tail & mask, mask == entries - 1 (power of two)
    assert ring.sq_mask == ring.sq_entries - 1
    assert ring.cq_mask == ring.cq_entries - 1
    assert ring.cq_tail.value - ring.cq_head.value == 0, "CQE leaked"


def scenario_readv(ring):
    buf = Buffer(4096)
    ring.live_buffers[0x21] = buf
    sqe = ring.get_sqe()
    prep_readv(sqe, fd=3, buf=buf, user_data=0x21)
    ring.submit(to_submit=1)
    cqe = ring.wait_cqe()
    ring.cqe_seen()
    assert cqe.user_data == 0x21
    assert cqe.res == 4096, "readv res %d != len" % cqe.res
    # only now is the buffer reusable
    ring.live_buffers.pop(0x21)
    assert ring.cq_tail.value - ring.cq_head.value == 0


def scenario_linked_timeout(ring):
    buf = Buffer(4096)
    ring.live_buffers[0xA1] = buf
    a = ring.get_sqe()
    prep_readv(a, fd=3, buf=buf, user_data=0xA1, slow=True, link=True)
    t = ring.get_sqe()
    prep_link_timeout(t, deadline_ticks=2, user_data=0xA2, target=0xA1)
    ring.submit(to_submit=2)
    c1 = ring.wait_cqe()
    ring.cqe_seen()
    c2 = ring.wait_cqe()
    ring.cqe_seen()
    by_ud = {c.user_data: c for c in (c1, c2)}
    assert 0xA1 in by_ud and 0xA2 in by_ud, "missing link-chain CQEs"
    # firing link timeout cancels the head request: all-or-nothing
    assert by_ud[0xA1].res == -62, "head res %d != -ETIME" % by_ud[0xA1].res
    assert by_ud[0xA2].res == -62, "timeout res %d != -ETIME" % by_ud[0xA2].res
    assert errno_of(by_ud[0xA1].res) == "ETIME"
    assert ring.cq_tail.value - ring.cq_head.value == 0


def scenario_multishot_accept(ring):
    ring.register_provided_buffers(buf_group=0, count=2)
    sqe = ring.get_sqe()
    prep_accept_multishot(sqe, fd=4, user_data=0xB1, buf_group=0)
    ring.submit(to_submit=1)
    got = []
    for _ in range(2):
        cqe = ring.wait_cqe()
        ring.cqe_seen()
        got.append(cqe)
    # two completions consumed both provided buffers; request is parked
    assert all(c.res == 16 for c in got), "accept res != 16"
    assert all(c.flags & IORING_CQE_F_MORE for c in got), "multishot MORE flag"
    assert ring.parked, "multishot accept must park when buffers are exhausted"
    # replenish the provided buffer ring -> the request re-arms and produces
    # one completion per freshly added buffer
    ring.add_provided_buffers(buf_group=0, count=2)
    re_armed = []
    for _ in range(2):
        cqe = ring.wait_cqe()
        ring.cqe_seen()
        re_armed.append(cqe)
    assert all(c.res == 16 for c in re_armed), "re-armed accept res"
    assert ring.cq_tail.value - ring.cq_head.value == 0


def run_scenario(name, fn, ring):
    try:
        fn(ring)
        violations = OrderAudit().check(ring)
        if violations:
            print("FAIL  %s: ordering audit: %s" % (name, violations))
            return False
        print("PASS  %s" % name)
        return True
    except RingError as exc:
        print("FAIL  %s: %s" % (name, exc))
        return False
    except AssertionError as exc:
        print("FAIL  %s: assertion: %s" % (name, exc))
        return False


def main():
    results = []
    results.append(run_scenario(
        "basic nop + ring geometry", scenario_basic_nop, Uring()))
    results.append(run_scenario(
        "readv completion + buffer lifetime", scenario_readv, Uring()))
    results.append(run_scenario(
        "linked timeout (all-or-nothing)", scenario_linked_timeout, Uring()))
    results.append(run_scenario(
        "multishot accept with provided buffers", scenario_multishot_accept,
        Uring()))
    if not all(results):
        print("RING PROTOCOL MODEL: FAIL")
        sys.exit(1)
    print("RING PROTOCOL MODEL: PASS")


if __name__ == "__main__":
    main()
