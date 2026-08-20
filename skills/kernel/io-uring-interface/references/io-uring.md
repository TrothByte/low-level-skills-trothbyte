# io_uring Ring Protocol Rules

Source-backed rule set for the io_uring submission/completion interface.
Each entry: RULE -> WHY AI GETS IT WRONG -> CORRECT REASONING -> EXAMPLE ->
COUNTEREXAMPLE -> VERIFICATION -> SOURCE. Confidence markers: KNOWN (primary
source), INFERRED (derived), UNVERIFIED (never rely on for correctness).
The host-verifiable core is the Python model in
`examples/good/ring_protocol.py` and `examples/bad/ring_misuse.py`.

## 0. The operation lifecycle (SQE -> enter -> CQE -> seen)

- **RULE**: Every io_uring operation is a state machine over the two shared
  rings. Submit path: `io_uring_get_sqe` (advance private tail) -> fill all
  SQE fields -> `io_uring_submit` (release store on `sq_tail` +
  `io_uring_enter`) -> kernel consumes SQEs `[sq_head, sq_tail)`. Complete
  path: `io_uring_wait_cqe` (acquire load on `cq_tail`) -> process CQE
  (`user_data`, `res`, `flags`) -> `io_uring_cqe_seen` (release store on
  `cq_head`). A buffer is owned by the kernel from submit until its matching
  CQE is consumed.
- **WHY AI GETS IT WRONG**: an agent treats the setup call as "just another
  async API", omits `io_uring_enter`/`io_uring_submit`, and then busy-loops
  reading CQEs that never arrive; or it treats a submitted request as already
  complete and reuses the buffer.
- **CORRECT REASONING**: `io_uring_setup` only creates the rings; nothing is
  submitted until the tail is published and `io_uring_enter` is called (raw
  API) or `io_uring_submit` wraps it (liburing). Completions appear only
  after the kernel processes SQEs and publishes CQEs by advancing `cq_tail`.
- **VERIFICATION**: `examples/good/ring_protocol.py` scenario "basic nop":
  get_sqe/prep/submit/wait/cqe_seen round-trips a NOP and asserts the CQ is
  empty afterwards.
- **SOURCE**: io_uring man pages (io_uring_setup, io_uring_enter); liburing
  `io_uring_submit`/`io_uring_wait_cqe`; LOTI chapters 1-2. KNOWN.

## 1. Index math: `idx = tail & mask`, never modulo, never byte-size masks

- **RULE**: SQ and CQ rings are power-of-two sized. The next slot is
  `tail & (entries - 1)`. `tail` is a monotonically increasing counter; the
  mask wraps it. `ring_size - 1` where `ring_size` is the ring *entry count*
  is the same thing only because the count is a power of two; using the ring
  *byte* size (`entries * sizeof(struct io_uring_sqe) - 1`) is wrong.
- **WHY AI GETS IT WRONG**: modulo arithmetic `tail % entries` appears
  equivalent in small userspace tests, and masking with byte sizes "works"
  until `tail` exceeds the ring size and the write lands out of bounds.
- **CORRECT REASONING**: `& mask` is a bitmask over a power-of-two modulus;
  it wraps exactly at `entries`. A byte-size mask wraps at `entries*64` and
  produces out-of-range indices on real rings.
- **VERIFICATION**: `examples/bad/ring_misuse.py` "ring index without mask"
  reproduces `tail & (entries*64 - 1)` and the checker flags the OOB index.
- **SOURCE**: io_uring_setup man page ring layout; liburing
  `io_uring_get_sqe` (`& *sq->sq_ring_mask`). KNOWN.

## 2. Memory ordering on the shared indices

- **RULE**: The SQ/CQ rings are a lock-free structure shared between the
  application and the kernel. The userspace contract:
  - `sq_tail` written with `smp_store_release` (after all SQE data is
    written) and read by the kernel;
  - `cq_head` written with `smp_store_release` (after all CQEs it covers are
    consumed) and read by the kernel;
  - `cq_tail` read with `smp_load_acquire` (before reading CQE data);
  - `sq_head` read with `smp_load_acquire` (to learn how many SQ slots are
    free).
  liburing wraps these (`io_uring_smp_store_release` / `io_uring_smp_load_acquire`).
- **WHY AI GETS IT WRONG**: in single-threaded userspace tests a plain
  assignment to `sq_tail` appears to work; the missing barrier only manifests
  under real contention (the kernel observes a tail pointing at not-yet-
  initialized SQEs, or the user observes a CQ tail without the CQE data).
- **CORRECT REASONING**: a release store pairs with the kernel's acquire load
  (and vice versa) to order the data accesses around the index update. The
  kernel side uses `WRITE_ONCE`/`READ_ONCE`-style accesses; userspace must
  provide the acquire/release half.
- **VERIFICATION**: `examples/good/ring_protocol.py` embeds an `OrderAudit`
  that fails if any user access to `sq_tail`/`cq_head` was not a release
  store or any read of `cq_tail`/`sq_head` was not an acquire load;
  `examples/bad/ring_misuse.py` "sq_tail plain store" reproduces the
  violation.
- **SOURCE**: io_uring man pages (memory ordering section); LOTI chapter 1;
  Linux kernel `io_uring/sync` handling. KNOWN.

## 3. Fully initialize every SQE field

- **RULE**: A fetched SQE must have all fields set before the tail is
  released: `opcode`, `flags`, `ioprio`, `fd`, `off`, `addr`, `len`,
  `rw_flags`, `user_data`, and (for receive paths) `buf_group`. liburing's
  `io_uring_prep_*` helpers zero the SQE first; hand-built SQEs must do the
  same.
- **WHY AI GETS IT WRONG**: agents set `opcode` and `user_data` and leave the
  rest "zero by default", not realizing the ring slot holds stale data from a
  previous submission.
- **CORRECT REASONING**: ring slots are recycled; an SQE is a struct the
  kernel reads in full. A single untouched field is garbage from the kernel's
  point of view (`fd`/`len`/`off` decide what the I/O touches).
- **VERIFICATION**: `examples/bad/ring_misuse.py` "uninitialized SQE fields"
  is rejected at submit time by the model's initializer check.
- **SOURCE**: io_uring man pages (struct io_uring_sqe fields); liburing
  prep helpers. KNOWN.

## 4. Buffer lifetime: never reuse before the matching CQE

- **RULE**: From submit until the matching CQE is consumed, the buffer (and
  the iovec pointing at it) belongs to the kernel. No reuse, no free, no
  frame-local ("stack") buffers.
- **WHY AI GETS IT WRONG**: agents reuse a "shared region" for the next
  request while the previous one is in flight, or pass a stack buffer "because
  readv returns immediately" — then see corrupted data or UAF (KASAN).
- **CORRECT REASONING**: completion is asynchronous; the kernel may be
  writing the buffer after the application has moved on. The CQE is the only
  release signal.
- **VERIFICATION**: `examples/bad/ring_misuse.py` "reuse buffer before CQE"
  and "stack buffer in flight" both end in the model's UAF check;
  `examples/good/ring_protocol.py` "readv completion + buffer lifetime" shows
  the legal ordering (buffer freed only after `cqe_seen`).
- **SOURCE**: io_uring man pages (io_uring_setup, "reaping events" notes);
  LOTI chapter 1. KNOWN.

## 5. Linked requests and linked timeouts are all-or-nothing

- **RULE**: `IOSQE_IO_LINK` makes the *next* SQE part of a chain. The chain
  executes as one unit: if any member fails, the remaining members are failed
  with `-ECANCELED` and do not execute. An `IORING_OP_LINK_TIMEOUT` armed
  after the head request fails the whole chain when it fires (head request
  cancelled, timeout itself completes `-ETIME`).
- **WHY AI GETS IT WRONG**: agents treat `IOSQE_IO_LINK` as "run these in
  parallel, independently" and expect the follower's CQE even when the head
  failed.
- **CORRECT REASONING**: linked is a dependency, not a queue. Failure
  propagates down the chain (`-ECANCELED`); a firing link timeout cancels the
  head request (`-ETIME`; newer kernels may report `-ECANCELED`). This is the
  kernel-side "all-or-nothing" semantics.
- **VERIFICATION**: `examples/good/ring_protocol.py` "linked timeout
  (all-or-nothing)" asserts head `-ETIME` and timeout `-ETIME`; the bad model
  reproduces the agent's wrong expectation.
- **SOURCE**: io_uring man pages (IOSQE_IO_LINK, io_uring_prep_link_timeout);
  io_uring FAQ (links and timeouts). KNOWN (semantics of a firing timeout
  changed across 5.5-5.15; see historical drift note in evals).

## 6. Provided buffer rings and multishot re-arm

- **RULE**: `IORING_REGISTER_PBUF_RING` registers a ring of buffers under a
  `buf_group` id; SQE `buf_group` selects it for receive paths. A multishot
  accept/recv re-arms itself: it keeps posting CQEs (flagged
  `IORING_CQE_F_MORE`) as events arrive, each consuming the next provided
  buffer. When the buffer ring empties, the request parks and resumes when
  the application replenishes buffers (`io_uring_update_buf_ring` /
  `io_uring_buf_ring_add` + advance).
- **WHY AI GETS IT WRONG**: agents treat multishot as one-shot; they drain
  one CQE and stop re-arming, or they register buffers without providing
  `buf_group`, or they assume provided buffers exist on kernels before 5.19.
- **CORRECT REASONING**: multishot is a loop inside the kernel: completion ->
  consume buffer -> wait next event. The user's job is to replenish buffers
  and to know when to stop via `IORING_CQE_F_MORE`/error (`-ECANCELED` on
  explicit stop, `-ENOBUFS` when no buffer is available).
- **VERIFICATION**: `examples/good/ring_protocol.py` "multishot accept with
  provided buffers": 2 completions consume 2 buffers, the request parks, then
  re-arms after replenishment.
- **SOURCE**: io_uring man pages (io_uring_register, IORING_REGISTER_PBUF_RING,
  IORING_ACCEPT_MULTISHOT); io_uring.h uapi. KNOWN (feature available from
  Linux 5.19).

## 7. `io_uring_enter` flags: IORING_ENTER_GETEVENTS

- **RULE**: `io_uring_enter(fd, to_submit, min_complete, flags, ...)`
  submits up to `to_submit` SQEs and/or waits for at least `min_complete`
  completions. `IORING_ENTER_GETEVENTS` tells the kernel to block until the
  requested completions exist.
- **WHY AI GETS IT WRONG**: agents call `io_uring_enter` with
  `to_submit>0` but no wait flag and then spin on the CQ; or they "submit"
  via `io_uring_setup` alone.
- **CORRECT REASONING**: submission and waiting are the same syscall with
  different argument meanings. `to_wait=0` never blocks. `io_uring_wait_cqe`
  (liburing) issues `io_uring_enter` with `IORING_ENTER_GETEVENTS`.
- **VERIFICATION**: model `submit(to_submit, to_wait)` mirrors the syscall
  signature; the bad model's "missing io_uring_enter (busy-loop)" scenario is
  the negative case.
- **SOURCE**: io_uring_enter man page. KNOWN.

## 8. Feature-gate everything (params.features)

- **RULE**: `io_uring_setup` returns `struct io_uring_params.features`
  describing what the running kernel supports (`IORING_FEAT_NODROP`,
  `IORING_FEAT_SINGLE_MMAP`, `IORING_FEAT_CQE_SKIP`, ...). Check the bits
  before relying on a feature; `io_uring_register` returns `-EINVAL` on old
  kernels. Check `io_uring_queue_init` return and `io_uring_*` error returns
  everywhere.
- **WHY AI GETS IT WRONG**: agents assume the newest kernel: they rely on
  `IORING_FEAT_NODROP` (5.5+), provided buffer rings (5.19+), multishot
  accept (5.19+), and skip error checking — then get silently lost CQEs or
  `-EINVAL` on realistic targets.
- **CORRECT REASONING**: io_uring is a moving interface; capability checks
  are part of correct initialization, not paranoia.
- **VERIFICATION**: `examples/bad/ring_misuse.py` "features assumed, not
  gated" reproduces an ignored `-EINVAL` from `io_uring_register`.
- **SOURCE**: io_uring_setup man page (features), io_uring_register man
  page, io_uring.h uapi. KNOWN.

## 9. io_uring is completion-based; epoll/aio are not

- **RULE**: epoll reports *readiness* (an fd is readable/writable); io_uring
  reports *completion* (an operation finished, with a result). libaio (aio_*)
  also reports completion but on a kernel-side context with a different
  submission path and no completion ring.
- **WHY AI GETS IT WRONG**: agents port epoll code to io_uring by mapping
  `EPOLLIN` onto an SQE and never handling results; or they call
  `io_uring_enter` after every event as if it were `epoll_wait` only.
- **CORRECT REASONING**: with io_uring you submit an *operation* (readv,
  accept, recv) once and it completes exactly once (or re-arms, in multishot);
  the CQE result is the outcome, not a readiness hint.
- **VERIFICATION**: conceptual; the model's scenarios always assert CQE
  results (res/errno) rather than readiness states.
- **SOURCE**: io_uring man pages; LOTI chapter 1; io_uring FAQ (comparison
  with epoll/aio). KNOWN.

## Historical drift notes (kernel-api-drift-migration)

- `IORING_FEAT_NODROP` added in Linux 5.5: before that, a full CQ dropped
  new completions. Agents writing for 5.1-5.4 must not rely on it.
- Linked-timeout cancellation: kernels through ~5.14 reported the cancelled
  head request as `-ETIME`; 5.15+ switched to `-ECANCELED` for cancelled
  requests. Expect either value in a review; never assert one exact errno
  for both eras. UNVERIFIED on this host (needs the kernel targets); KNOWN
  from io_uring changesets/FAQ.
- Provided buffer rings (`IORING_REGISTER_PBUF_RING`) and `IORING_ACCEPT_MULTISHOT`
  landed in 5.19; multishot recv without provided buffers came earlier.
- `io_uring_setup` `IORING_FEAT_SINGLE_MMAP` lets SQ and CQ share one mmap;
  without it the rings must be mmapped separately.
