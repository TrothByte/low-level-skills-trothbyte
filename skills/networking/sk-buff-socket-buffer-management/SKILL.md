---
name: sk-buff-socket-buffer-management
description: Use when writing or reviewing Linux networking code that manipulates struct sk_buff — headroom/tailroom/data layout, skb_put/skb_push/skb_reserve/skb_pull, skb_clone vs skb_copy vs skb_share_check, ownership, and freeing. Teaches the sk_buff buffer model so code never corrupts the data area or leaks/frees wrongly.
---

# sk_buff Socket Buffer Management

Rules for building, parsing, cloning, and freeing `struct sk_buff` without
corrupting the data area or leaking/over-freeing buffers. Load
`references/README.md` before touching `skb_put`, `skb_push`, `skb_reserve`,
or `skb_clone`.

## When to use

- Writing or reviewing kernel networking code that builds or parses packets:
  `ndo_start_xmit`, receive path, netfilter, protocol handlers.
- Building headers with `skb_reserve` + `skb_push` and payload with `skb_put`.
- Advancing `skb->data` with `skb_pull` when parsing.
- Choosing between `skb_clone`, `skb_copy`, and `skb_share_check`.
- Freeing with `consume_skb` / `kfree_skb` and reasoning about ownership.
- Tracing skb leaks or refcount bugs in network drivers.
- Reviewing skb-manipulating netfilter or driver patches.
- Reviewing patches for CWE-787 (OOB write), CWE-190 (overflow), CWE-415
  (double free), CWE-416 (use after free).

## When not to use

- User-space or non-networking code with no `struct sk_buff`.
- Other kernels: BSD `mbuf` and Windows `NET_BUFFER` buffer models differ.
- Performance tuning of a correct datapath; this skill is about correctness.
- API lookup: the kernel docs cover contracts; this file is the reasoning
  layer.
- TCP stack internals around `sk_buff` queues live in
  `tcp-congestion-control-internals`.

## What the agent often gets wrong

- Calling `skb_put` when tailroom is insufficient. `skb_put` never expands the
  allocation; the write runs past `skb->tail` into tailroom and slab.
- Forgetting `skb_reserve` before building a header, so `skb_push` writes
  before `skb->head`.
- Treating `skb_clone` as a deep copy: the data area is SHARED; only the
  `sk_buff` struct and control block (`cb`) are copied.
- Using `skb->data` after `skb_pull` / `skb_push` without recomputation —
  cached pointers go stale because `skb->data` moved.
- Writing into the data area of a shared clone as if private, corrupting the
  original's payload.
- Freeing a shared skb from wrong ownership: each holder drops one reference;
  double-free / use-after-free follow when two holders both free.
- Skipping validation of packet-derived lengths before `skb_put` / `skb_pull`;
  on the wire these are attacker-controlled.

## How to reason correctly

1. Four pointers over one allocation: `head <= data <= tail <= end` with
   `headroom = data - head`, `len = tail - data`, `tailroom = end - tail`.
2. `skb_reserve(len)` sets `data = tail = head + len`; call it right after
   allocation to create headroom for later headers.
3. `skb_put(len)` appends: moves `tail` toward `end`, returns the old tail,
   never grows the allocation. Contract: `len <= skb_tailroom()`.
4. `skb_push(len)` prepends: moves `data` back toward `head`; requires
   `len <= skb_headroom()`.
5. `skb_pull(len)` advances `data` when a header is consumed; re-read the
   returned pointer, never keep a stale copy.
6. Clone vs copy vs share-check: `skb_clone` copies struct + `cb`, shares the
   data area (refcount +1); `skb_copy` duplicates the whole buffer (private
   data); `skb_share_check` clones only if shared. Modify data bytes only
   after a copy.
7. Ownership: exactly one owner frees. Receive path owns until consumed;
   transmit frees after xmit. `skb_get` bumps `skb->users`; each holder
   `consume_skb`/`kfree_skb` exactly once. Data dies with the last reference.
8. Treat packet-derived lengths as untrusted: validate with overflow-safe
   arithmetic before any `skb_put` / `skb_pull`.
9. `skb_put`/`skb_push`/`skb_pull` act on the linear area only; page
   fragments (`skb_shinfo()->frags`) are managed separately.

Worked trace (128-byte allocation, encoded in the examples): `alloc_skb(128)`
-> headroom/len/tailroom 0/0/128; `skb_reserve(32)` -> 32/0/96; `skb_put(64)`
-> 32/64/32; `skb_push(4)` -> 28/68/32; `skb_pull(4)` -> 32/64/32;
headroom + len + tailroom stays 128.

## What to verify

- Every `skb_put` length `<= skb_tailroom()`; every `skb_push` length
  `<= skb_headroom()`.
- `skb_reserve` right after allocation, before building headers.
- `skb->data` re-read after `skb_pull` / `skb_push`.
- Shared skb data never written without a private copy.
- Each skb freed exactly once by its owner; `skb->users` transitions match
  the clone/copy calls.
- No `skb_put` / `skb_pull` with unvalidated packet-derived lengths.
- Invariant `head <= data <= tail <= end` holds at every check point.
- Linear-area operations never applied to paged fragments.

## How to verify

Host-compilable logic checks (self-contained stubs, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_skb.c -o /tmp/good_skb
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_skb.c -o /tmp/bad_skb
./good_skb   # prints ALL CHECKS PASSED, exit 0
./bad_skb    # prints the three BUG reproduced lines, exit 0
```

Target (kernel) checks — document these, do not claim to have run them:

```
# KASAN kernel build (skb_put tailroom overflow -> redzone fault)
make defconfig && make -j$(nproc)   # CONFIG_KASAN=y
qemu-system-x86_64 -kernel arch/x86/boot/bzImage \
  -append "console=ttyS0 kasan=on" -nographic

# net selftests / syzkaller fuzzing of skb-heavy paths
make -C tools/testing/selftests TARGETS=net
```

## Where the knowledge comes from

- `linux-skbuff-docs` — struct layout, headroom/data/tailroom model,
  skb_put/push/pull/reserve contracts
- `linux-networking-docs` — buffer ownership, clone/copy/share-check
  semantics and "who frees" rules
- `kernel-source` — net/core/skbuff.c clone/copy/free and the `skb->users`
  refcount
- `linux-napi-docs` — receive-path ownership handoff into and out of NAPI
- `ldd3` — network driver patterns, xmit/free responsibilities
- `cwe` — CWE-787/190/415/416 weakness classes behind skb misuse
- `nvd-cve` — CVE-2021-43267 (TIPC skb length handling) and the skb_put
  tailroom-overflow bug class

## Related skills

- `napi-network-driver` — receive path that owns skbs into and out of NAPI
- `tcp-congestion-control-internals` — skb-manipulating TCP stack internals
- `kernel-atomic-context` — what is legal while holding skb locks
- `c-string-and-buffer-safety` — bounds discipline for header fields
- `memory-ordering-reasoning` — ordering of skb field updates
- `kernel-rcu-memory-barriers` — RCU semantics near skb paths

## Evaluation

Historical CVEs: CVE-2021-43267 — TIPC crypto key exchange (`net/tipc/crypto.c`):
message length fields trusted, wrong length arithmetic preceded `skb_put`, and
the write ran past the allocation into adjacent heap memory; fixed by
validating the length before `skb_put`. Documented class only (KNOWN, NVD).
Same class: writing past `skb->tail` into tailroom/slab when `skb_put` is
called with insufficient tailroom — `skb_put` never expands the buffer (KNOWN,
kernel-source). Synthetic: `skb_put` past tailroom, `skb_push` without reserve,
`skb_pull` beyond data, write into a shared clone's data, double free of a
shared skb. Adversarial: code that passes a single-owner smoke test but
corrupts data once cloned, or parses lengths from untrusted packet fields.
False-positive: correct reserve+put+push+pull sequences, private copies, and
single-owner frees must NOT be flagged.
