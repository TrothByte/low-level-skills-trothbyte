# Evaluation — io-uring-interface

Skill: `skills/kernel/io-uring-interface`. Stability target: `researched`.
The ring protocol core is host-verified (Python model); kernel-side semantics
(specific errnos, feature bits, multishot behavior on real hardware) require
the Linux 5.1+ targets listed below and are documented, NOT claimed to have
been run.

## Verified facts (host, recorded 2026-08-20)

Recorded output of the host-run Python models (Windows 10, Python 3.11.9,
run from the skill directory):

```
> python examples/good/ring_protocol.py
PASS  basic nop + ring geometry
PASS  readv completion + buffer lifetime
PASS  linked timeout (all-or-nothing)
PASS  multishot accept with provided buffers
RING PROTOCOL MODEL: PASS

> python examples/bad/ring_misuse.py
BUG reproduced: SQE slot 0 submitted with uninitialized field(s): fd, len
BUG reproduced: ring index 16 computed with byte-size mask (entries*64-1); must be tail & (sq_entries-1)
BUG reproduced: buffer for user_data 0x2 freed/reused before the matching CQE was read (stack buffer or reuse-before-CQE)
BUG reproduced: buffer for user_data 0x3 freed/reused before the matching CQE was read (stack buffer or reuse-before-CQE)
BUG reproduced: SQEs fetched but io_uring_enter() never called — the kernel cannot see them and the CQ never advances (agent busy-loops on wait_cqe)
BUG reproduced: sq_tail written without smp_store_release
BUG reproduced: IOSQE_IO_LINK chain failed all-or-nothing: head got -22 and the follower was failed with -125 instead of completing independently
BUG reproduced: io_uring_register(IORING_REGISTER_PBUF_RING) returned -22: provided buffers assumed without checking params.features on an old kernel
RING MISUSE MODEL: PASS — 8 bug classes caught
```

Verified on host: power-of-two ring geometry, `tail & mask` index math, the
release/acquire ordering contract on sq_tail/cq_head/cq_tail/sq_head
(OrderAudit in the good model, plain-store violation in the bad model), the
all-or-nothing linked-chain failure propagation, linked-timeout firing
(-ETIME on head and timeout), provided-buffer consumption + multishot re-arm
after replenishment, and CQE `res < 0` as `-errno`.

NOT verified on this host (documented targets, do NOT claim to have run):
real `io_uring_setup`/`io_uring_enter` syscalls, liburing runs, CQ overflow
behavior on real 5.1-5.4 kernels, pbuf-ring behavior on real 5.19 kernels.

## Synthetic evals

The four good-model scenarios assert the protocol is followed and PASS:

- easy/positive: NOP round-trip + ring geometry (mask == entries - 1).
- easy/positive: readv completes with `res == len`; buffer reused only after
  `cqe_seen`.
- medium/positive: linked readv + link timeout — head cancelled `-ETIME`,
  timeout `-ETIME`, no independent completion, CQ drained.
- hard/positive: multishot accept with a provided buffer ring — 2 completions
  consume 2 buffers, request parks when empty, re-arms after replenishment.

The eight bad-model scenarios are the negative cases: uninitialized SQE,
byte-size mask, reuse-before-CQE, stack buffer, missing `io_uring_enter`
(busy-loop), plain store on `sq_tail`, link chain treated as independent,
features assumed instead of gated.

## False-positive evals

Correct code must NOT be flagged:

- SQE fully initialized via a prep helper (all fields set before submit).
- `submit(to_submit, to_wait)` where `to_wait == 0` for fire-and-forget.
- Buffers freed only after their matching CQE is consumed.
- A `& mask` index computation with a power-of-two entry count.
- Release store on `sq_tail` (through `submit`) and on `cq_head` (through
  `cqe_seen`); acquire loads on `cq_tail` and `sq_head`.
- Feature-gated use of `IORING_FEAT_NODROP` / provided buffer rings.
- Non-io_uring completion code (libaio) reviewed with the io_uring lens
  must not trigger io_uring-specific diagnostics.

## Historical evals

- io_uring introduced in Linux 5.1 (io_uring_setup/enter/register, SQ/CQ
  mmap rings). Agents must not require later features on 5.1-5.4 kernels.
- `IORING_FEAT_NODROP` added in Linux 5.5: earlier kernels silently drop
  CQEs on overflow. The bad-model "CQ overflow without NODROP" path models
  the failure mode; feature-gating is the fix.
- Linked-timeout semantics drifted: firing timeouts cancelled the head
  request with `-ETIME` on earlier kernels, `-ECANCELED` from ~5.15 for
  cancelled requests. The model asserts the earlier convention and the
  reference documents the drift (see `references/io-uring.md`). DETECT ->
  EXPLAIN -> FIX -> VERIFY: catch an agent asserting a single exact errno
  across both eras.
- Provided buffer rings (`IORING_REGISTER_PBUF_RING`) and
  `IORING_ACCEPT_MULTISHOT` landed in 5.19; a review must gate them.

## Adversarial evals

- An agent that "fixes" the missing-enter bug by busy-looping wait_cqe
  without `IORING_ENTER_GETEVENTS` — the model catches the no-enter path.
- An agent that adds `IOSQE_IO_LINK` to make two requests "sequential" while
  reusing the same buffer for both — linked semantics + buffer lifetime
  combined.
- Code that "works" in a userspace-only test: plain assignment to `sq_tail`
  (no release) or byte-size masks that only misbehave after wrap — the
  OrderAudit and index checker catch both where a smoke test would not.
- Multishot accept draining one CQE and stopping, leaving the request armed
  and the provided buffer ring unreplenished.

## Verification commands (target — Linux 5.1+ with liburing)

Documented, NOT run on the authoring host:

```
gcc -O2 -Wall -Wextra examples/good/good_uring_basic.c -o /tmp/good_basic -luring
gcc -O2 -Wall -Wextra examples/good/good_uring_linked_timeout.c -o /tmp/good_lt -luring
# good_uring_multishot_accept needs Linux 5.19+ and liburing >= 0.7:
gcc -O2 -Wall -Wextra examples/good/good_uring_multishot_accept.c -o /tmp/good_ms -luring
# run good_ms with a listening socket on fd 3 (e.g. via a wrapper that
# dup2()s a bound+listening socket to 3) so the multishot accept has work.
```

Target checks for the bug classes (KASAN build under QEMU recommended):
`examples/bad/bad_uninitialized_sqe.c`, `bad_stack_buffer.c`,
`bad_reuse_before_cqe.c`, `bad_index_mask.c` — each should fail under KASAN
or produce garbage results, matching the model diagnostics.

Host re-runs after any change to the models:

```
python examples/good/ring_protocol.py
python examples/bad/ring_misuse.py
python ../../../../tools/lint/skill_lint.py SKILL.md
```

## Scoring

- 0 = skill would mislead: a protocol rule above is wrong (index math,
  ordering, linked semantics, multishot re-arm, feature gating) and the
  models cannot be made to expose it.
- 1 = one non-core claim (e.g. an exact errno for a specific kernel era) is
  wrong, but the protocol model stays correct.
- 2 = all model scenarios PASS and all bug classes are caught on the host;
  kernel-side claims are marked UNVERIFIED/`researched` and explicitly
  require the documented Linux targets.
- 3 = additionally run on Linux 5.1+ with liburing: the three good C examples
  build and behave as documented, and the bad C examples reproduce their bugs
  under KASAN.
