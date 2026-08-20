---
name: io-uring-interface
description: Use when writing, reviewing, or debugging Linux async I/O with io_uring — ring setup, SQE/CQE handling, linked requests and timeouts, provided buffer rings, multishot operations, or when an agent confuses io_uring with epoll/aio. Teaches the ring protocol and its memory-ordering rules.
---

# io_uring Interface

io_uring (Linux 5.1+) is a completion-based async I/O interface built on two
lock-free shared-memory rings: a submission queue (SQ, 64-byte SQEs) and a
completion queue (CQ, 16-byte CQEs), created by `io_uring_setup`, driven by
`io_uring_enter`, and configured by `io_uring_register`. The ring protocol
and its memory-ordering rules are the verifiable core of this skill; the
host-run Python model in `examples/good/ring_protocol.py` reproduces them.

## When to use

- Writing or reviewing code that uses `io_uring_setup` / `io_uring_enter` /
  `io_uring_register`, or the liburing helpers that wrap them.
- Debugging io_uring failures: wrong results, lost completions, `-EFAULT` /
  `-EINVAL` / `-ECANCELED` / `-ETIME` on the CQ, or busy-loops on the CQ.
- Designing high-performance I/O paths: fixed files, provided buffer rings,
  multishot accept/recv, SQPOLL.
- Reasoning about linked requests (`IOSQE_IO_LINK`) and linked timeouts.
- Reviewing code where an agent confuses io_uring with epoll (readiness) or
  libaio (kernel-context completions).

## When not to use

- Readiness-based event loops (`epoll_wait`, `select`, `poll`).
- Synchronous or thread-per-request I/O with no submission queue.
- Non-Linux kernels: io_uring is Linux-specific (FreeBSD `io_uring` and
  liburing-for-libuv variants differ).
- `libaio` (`aio_*`) — same completion idea, different submission model and
  no userspace completion ring.
- Kernel-internal code: io_uring is a userspace-facing syscall interface;
  kernel modules use `uring_cmd` or block-layer APIs instead.

## What the agent often gets wrong

- Treating io_uring as a blocking API: missing `io_uring_enter` /
  `io_uring_submit`, then busy-looping on the CQ that never advances.
- Forgetting to initialize all SQE fields — the kernel reads garbage from
  recycled ring slots (stale `fd`/`len`/`off`).
- Wrong index math: `tail & (ring_bytes - 1)` instead of `tail & (entries - 1)`,
  or missing the release/acquire ordering on the shared head/tail indices —
  the bug hides in userspace-only tests and only breaks under contention.
- Reusing a buffer (or an iovec) before its matching CQE is consumed, and
  passing stack buffers to async operations (use-after-free).
- Treating `IOSQE_IO_LINK` chains as independent requests: a failed head
  fails the whole chain with `-ECANCELED`.
- Assuming every kernel supports every opcode and feature (provided buffer
  rings, multishot, `IORING_FEAT_NODROP`).
- Confusing io_uring with epoll: epoll is readiness-based, io_uring is
  completion-based.

## How to reason correctly

1. Map the operation lifecycle: SQE fetch -> fill all fields -> release store
   on `sq_tail` -> `io_uring_enter` -> kernel consumes and completes -> CQE
   acquired on `cq_tail` -> process -> release store on `cq_head` -> buffer
   reusable.
2. Always compute ring indices with `idx = tail & mask` (rings are
   power-of-two in entries); keep head/tail counters monotonic in a struct.
3. Use release stores on `sq_tail` and `cq_head`, acquire loads on `cq_tail`
   and `sq_head`. This is a lock-free shared structure with the kernel; the
   ordering is the contract.
4. For high-performance servers prefer fixed files + provided buffers +
   multishot accept/recv; fall back to plain readv/writev/accept for
   correctness first, then optimize.
5. Feature-gate everything: check `io_uring_setup` `params.features`
   (`IORING_FEAT_NODROP`, `IORING_FEAT_SINGLE_MMAP`, ...) and every
   `io_uring_register` / `io_uring_queue_init` return before relying on a
   capability.

## What to verify

- Ring indices computed with `& mask`; release store on `sq_tail`; acquire
  loads on `cq_tail`/`sq_head`; `cq_head` re-committed after processing.
- Every SQE fully initialized (opcode, flags, ioprio, fd, off, addr, len,
  rw_flags, user_data, buf_group); buffers and iovecs live until the matching
  CQE is seen.
- `io_uring_enter` called with the correct `to_submit` / `to_wait`, with
  `IORING_ENTER_GETEVENTS` when waiting; CQE `res < 0` treated as `-errno`.
- No stack buffers in flight, no reuse before CQE, linked chains fail
  all-or-nothing, multishot requests keep re-arming / get replenished buffers.

## How to verify

Host-run Python model (the verifiable core — runs on any host, no kernel):

```
python examples/good/ring_protocol.py    # 4 protocol scenarios, asserts PASS
python examples/bad/ring_misuse.py       # 8 bug classes, prints "BUG reproduced"
```

Target (Linux 5.1+ with liburing) — documented, NOT run on this Windows host:

```
gcc -O2 -Wall -Wextra examples/good/good_uring_basic.c -o /tmp/good_basic -luring
gcc -O2 -Wall -Wextra examples/good/good_uring_linked_timeout.c -o /tmp/good_lt -luring
gcc -O2 -Wall -Wextra examples/good/good_uring_multishot_accept.c -o /tmp/good_ms -luring
# run with a listening socket on fd 3 for the multishot accept example
```

## Where the knowledge comes from

- io_uring documentation (https://kernel.docs.io/uring or https://docs.kernel.org/), io_uring man pages (https://man.7z.org/io_uring)
- liburing — the reference library (https://github.com/axboe/liburing)
- The Linux Kernel API — io_uring section (https://docs.kernel.org/), io_uring_register/setup/enter man pages
- Jens Axboe io_uring tutorial (https://unixism.net/loti/)

## Related skills

- `kernel-uaccess-safety` — user/kernel memory rules; io_uring buffers face
  the same lifetime questions as `copy_to_user` arguments.
- `kernel-api-drift-migration` — feature and errno drift across io_uring
  kernel versions (NODROP, linked-timeout `-ETIME`/`-ECANCELED`, pbuf rings).
- `networking-hardware-rdma-nic-offload` — high-throughput I/O paths that
  io_uring-based servers sit on top of.
- `hpc-rdma-verbs` — completion-queue programming patterns that map onto the
  io_uring SQ/CQ model.
- `kernel-atomic-context` — what is legal where; relevant for drivers behind
  `uring_cmd` and for SQPOLL thread constraints.
- `concurrency-actual-parallelism-detection` — spotting busy-loops and missed
  synchronization in async paths.

## Evaluation

Stability: `researched` — the ring protocol, index math, memory ordering,
linked-chain, multishot and feature-gating claims are modelled and host-
verified in Python; kernel-side semantics require the Linux 5.1+ targets
documented in `evals/README.md`. Synthetic evals: the four good-model
scenarios and eight bad-model bug classes. Historical evals: `IORING_FEAT_NODROP`
introduction (5.5), linked-timeout cancellation change (5.15), pbuf-ring /
multishot introduction (5.19). Adversarial: agents that "fix" one bug by
violating another (e.g. adding `IOSQE_IO_LINK` while reusing buffers). See
`evals/README.md` for the recorded 2026-08-20 host output and scoring.
