# Linux Kernel sk_buff Rules

Source-backed rule set for `struct sk_buff` buffer management. Each entry:
RULE -> WHY AI GETS IT WRONG -> CORRECT REASONING -> EXAMPLE -> COUNTEREXAMPLE
-> VERIFICATION -> SOURCE. Confidence markers: KNOWN (documented contract),
INFERRED (derived), UNVERIFIED (never use in a stable skill).

## 1. The sk_buff layout: four pointers, one allocation

- **RULE**: An skb owns a single buffer described by four pointers with the
  invariant `head <= data <= tail <= end`. `headroom = data - head`,
  `len = tail - data`, `tailroom = end - tail`. All pointer moves happen
  inside these bounds.
- **WHY AI GETS IT WRONG**: agents think of an skb as "a buffer with a
  length" and apply `memcpy`/realloc habits, ignoring that data has a
  start (headroom) and an end (tailroom) with distinct contracts.
- **CORRECT REASONING**: the headroom is reserved for headers to be pushed
  later, the middle is the current data, the tailroom is room to append.
  Any access outside `[head, end)` is memory corruption; any access outside
  `[data, tail)` reads garbage or crosses into another owner's region.
- **EXAMPLE** (bad):
  ```c
  memcpy(skb->data + skb_len(skb), src, n);  /* may write past tail */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (n > skb_tailroom(skb))
      return -ENOMEM;
  memcpy(skb_tail_pointer(skb), src, n);     /* inside [data, end) */
  skb_put(skb, n);
  ```
- **VERIFICATION**: harness asserts `head <= data <= tail <= end` after every
  operation; `gcc -Wall -Wextra -Werror -O2` on the stubbed examples.
- **SOURCE**: linux-skbuff-docs (skb layout); kernel-source (net/core/skbuff.c);
  cwe (CWE-787).

## 2. Headroom and `skb_reserve`

- **RULE**: `skb_reserve(skb, len)` moves `data` and `tail` to `head + len`,
  creating `len` bytes of headroom. Call it immediately after allocation so
  that later `skb_push` calls have room for protocol headers.
- **WHY AI GETS IT WRONG**: headers are built with `skb_push` before any
  headroom exists, writing before `skb->head`.
- **CORRECT REASONING**: reserve-then-push is the canonical order:
  `alloc_skb(size)` -> `skb_reserve(HEADROOM)` -> `skb_put(payload)` ->
  `skb_push(hdr_len)` per layer. The reserved headroom is shared by all
  protocol layers; each push adds a header in front.
- **EXAMPLE** (bad):
  ```c
  struct sk_buff *skb = alloc_skb(1500, GFP_ATOMIC);
  skb_push(skb, 14);   /* no reserve: writes before skb->head */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  struct sk_buff *skb = alloc_skb(1500, GFP_ATOMIC);
  if (!skb) return -ENOMEM;
  skb_reserve(skb, 64);        /* headroom for device/host headers */
  skb_put(skb, payload_len);
  skb_push(skb, 14);           /* header goes into the headroom */
  ```
- **VERIFICATION**: harness: push without reserve hits the headroom BUG;
  reserve-then-push preserves headroom invariants.
- **SOURCE**: linux-skbuff-docs (skb_reserve); linux-networking-docs (tx path);
  ldd3 (network drivers); cwe (CWE-787).

## 3. `skb_put`: grow the tail, never beyond the tailroom

- **RULE**: `skb_put(skb, len)` appends `len` bytes to the data area: `tail`
  moves toward `end` and the old tail pointer is returned. The caller MUST
  guarantee `len <= skb_tailroom(skb)`; `skb_put` never expands the
  allocation.
- **WHY AI GETS IT WRONG**: agents treat `skb_put` like `memcpy` into a
  growable buffer, so wrong length arithmetic (e.g. from packet header
  fields) makes the write run past `tail` into the tailroom and the slab.
- **CORRECT REASONING**: the allocation size is fixed at `alloc_skb` time.
  `skb_put` is a pure pointer advance; overflowing it corrupts adjacent
  memory (the documented `skb_put` tailroom-overflow bug class, KNOWN).
  Validate lengths before calling: prefer `skb_put` only after
  `skb_tailroom()` is known to be sufficient.
- **EXAMPLE** (bad):
  ```c
  skb_put(skb, ntohs(hdr->payload_len));  /* attacker-controlled, unchecked */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  unsigned int n = ntohs(hdr->payload_len);
  if (n > skb_tailroom(skb))
      return -EMSGSIZE;         /* validate BEFORE growing the tail */
  unsigned char *dst = skb_put(skb, n);
  ```
- **VERIFICATION**: harness: `skb_put` beyond tailroom raises the BUG
  diagnostic and returns NULL; KASAN target reports the overflow as a
  redzone fault.
- **SOURCE**: linux-skbuff-docs (skb_put contract); kernel-source (skbuff.c);
  nvd-cve (CVE-2021-43267 class); cwe (CWE-787, CWE-190).

## 4. `skb_push`: prepend a header into the headroom

- **RULE**: `skb_push(skb, len)` moves `data` back toward `head` and returns
  the new `data`. It is the header-building primitive and requires
  `len <= skb_headroom(skb)`.
- **WHY AI GETS IT WRONG**: pushes are chained per protocol layer without
  accounting for the headroom each layer consumes, exhausting it silently.
- **CORRECT REASONING**: each protocol layer adds its header in front of the
  data, consuming headroom. The total pushed bytes across all layers must fit
  the reservation made by `skb_reserve`. Pushing more than the headroom
  writes before `skb->head`.
- **EXAMPLE** (bad):
  ```c
  skb_push(skb, 20);   /* ETH+IP+UDP headers but only 14 bytes reserved */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (skb_headroom(skb) < hdr_len)
      return -ENOBUFS;            /* or pskb_expand_head on the target */
  unsigned char *hdr = skb_push(skb, hdr_len);
  ```
- **VERIFICATION**: harness: push past headroom hits the BUG diagnostic and
  returns NULL; the reserved-headroom flow stays in bounds.
- **SOURCE**: linux-skbuff-docs (skb_push); linux-networking-docs; cwe
  (CWE-787).

## 5. `skb_pull`: advance `data` when parsing, and re-read it

- **RULE**: `skb_pull(skb, len)` advances `data` by `len` (a consumed header)
  and returns the new `data` pointer. After a pull, `skb->data` has moved:
  any cached copy of the old pointer is stale.
- **WHY AI GETS IT WRONG**: agents cache `skb->data` once, then `skb_pull`
  and keep parsing with the stale pointer, re-reading header bytes that are
  now past the data area.
- **CORRECT REASONING**: parsing is a walk: `data` starts at the outer header
  and each `skb_pull` advances it to the next inner header. Always take the
  return value (`skb_pull` returns the new pointer; on underflow newer
  kernels return NULL) and never trust a pointer captured before the pull.
- **EXAMPLE** (bad):
  ```c
  unsigned char *ip = skb->data;
  skb_pull(skb, 14);
  unsigned char proto = ip[9];   /* stale: points at the Ethernet header */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  skb_pull(skb, 14);
  unsigned char *ip = skb->data;  /* recomputed after the pull */
  unsigned char proto = ip[9];
  ```
- **VERIFICATION**: harness: after `skb_pull_emu` the returned pointer equals
  `data`; stale-pointer access is flagged in review.
- **SOURCE**: linux-skbuff-docs (skb_pull); linux-networking-docs; cwe
  (CWE-787).

## 6. `skb_clone`: struct and control block copied, data SHARED

- **RULE**: `skb_clone(skb, gfp)` creates a new `sk_buff` whose struct fields
  and control block (`cb`) are copied but whose data area is the same
  allocation: the data refcount is incremented and both skbs set the shared
  flag. Modifying data bytes through a clone modifies the original too.
- **WHY AI GETS IT WRONG**: agents read "clone" as "deep copy" and then write
  into `clone->data`, corrupting the original's payload.
- **CORRECT REASONING**: a clone exists so two owners can hold the same packet
  (broadcast, GRO/LRO, netfilter reinjection) while each may still touch its
  own struct and `cb`. It is NOT a private data copy. If the data must be
  modified, use `skb_copy` or `pskb_copy` instead.
- **EXAMPLE** (bad):
  ```c
  struct sk_buff *c = skb_clone(skb, GFP_ATOMIC);
  memset(c->data, 0, 64);        /* corrupts skb's payload */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  struct sk_buff *c = skb_clone(skb, GFP_ATOMIC);
  memcpy(c->cb, skb->cb, sizeof(c->cb));  /* struct-level data only */
  /* if payload bytes must change: use skb_copy / pskb_copy first */
  ```
- **VERIFICATION**: harness asserts `clone->data == skb->data` and
  `dataref == 2` after clone, and that a cb write does not leak into the
  original.
- **SOURCE**: linux-skbuff-docs (skb_clone); kernel-source (net/core/skbuff.c
  `__skb_clone`); linux-napi-docs; cwe (CWE-787).

## 7. `skb_copy`: the whole buffer is duplicated

- **RULE**: `skb_copy(skb, gfp)` allocates a new buffer and copies the entire
  data area, returning a fully private skb with `dataref == 1`. Use it when
  the data bytes themselves will be modified.
- **WHY AI GETS IT WRONG**: `skb_clone` is used where a copy is needed, or
  `skb_copy` is used where a clone would do, paying a full allocation and
  copy for no reason.
- **CORRECT REASONING**: the choice is driven by what is modified: only the
  struct/`cb` -> clone; data bytes -> copy. `skb_copy` is expensive (full
  alloc + memcpy), so it is only justified on the data-mutation path.
- **EXAMPLE** (bad):
  ```c
  struct sk_buff *c = skb_clone(skb, GFP_ATOMIC);
  if (need_translate) {
      for (i = 0; i < c->len; i++)
          c->data[i] ^= key;     /* corrupts the original */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  struct sk_buff *c = skb_copy(skb, GFP_ATOMIC);
  if (!c) return -ENOMEM;
  for (i = 0; i < c->len; i++)
      c->data[i] ^= key;         /* private data, original untouched */
  ```
- **VERIFICATION**: harness: after `skb_copy_emu`, `dataref == 1` and
  modifying the copy leaves the original's bytes unchanged.
- **SOURCE**: linux-skbuff-docs (skb_copy vs skb_clone); kernel-source
  (skbuff.c); cwe (CWE-787).

## 8. `skb_share_check`: clone only when the data is shared

- **RULE**: `skb_share_check(skb, pri)` returns a cloned skb when the data
  area is shared (`dataref != 1`), dropping this holder's reference to the
  original; otherwise it returns `skb` unchanged. It unshares the STRUCT, not
  the data — an optimization for owners that only touch struct fields.
- **WHY AI GETS IT WRONG**: agents believe `skb_share_check` gives a private
  data copy and write into the returned skb's data, still corrupting the
  other holder.
- **CORRECT REASONING**: share-check exists to avoid an unconditional clone on
  the common single-owner path. After it, the caller owns the returned
  `sk_buff` struct exclusively, but if the data was shared the bytes are
  still aliased — data mutation still requires a copy.
- **EXAMPLE** (bad):
  ```c
  struct sk_buff *s = skb_share_check(skb, GFP_ATOMIC);
  s->data[0] = 0xFF;   /* data may still be shared -> corrupts original */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  struct sk_buff *s = skb_share_check(skb, GFP_ATOMIC);
  s->priority = 7;     /* struct field: safe after unsharing */
  ```
- **VERIFICATION**: harness: `skb_share_check_emu` re-clones and consumes the
  old reference when `dataref > 1`, and returns the skb untouched when not
  shared.
- **SOURCE**: linux-skbuff-docs (skb_share_check); kernel-source (skbuff.c);
  cwe (CWE-787).

## 9. `skb->users` refcount and the single-owner rule

- **RULE**: `skb->users` counts references to the sk_buff struct.
  `skb_get(skb)` increments it; each increment must be matched by exactly one
  `consume_skb` / `kfree_skb`. Only the current owner may free, and only the
  owner may decide to hand ownership on.
- **WHY AI GETS IT WRONG**: two code paths both free the "same" skb (each
  thinks it owns it), causing a double free or a use-after-free when the
  first owner already released the buffer.
- **CORRECT REASONING**: ownership is a hand-off, not a share: the receive
  path owns the skb until it is delivered to a protocol/socket; the transmit
  path frees after xmit. Cloning adds references — each clone is a new
  owner that must drop its own reference. The data area is freed when the
  last reference drops, not when the first does.
- **EXAMPLE** (bad):
  ```c
  /* tx path */ dev_hard_start_xmit(skb, dev);   /* device frees skb */
  /* same path */ kfree_skb(skb);                /* double free */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (dev_queue_xmit(skb))     /* hands ownership to the queue */
      return NETDEV_TX_OK;     /* do NOT free here */
  ```
- **VERIFICATION**: harness tracks `users` through clone/copy/free and
  flags any free of an already-freed struct.
- **SOURCE**: kernel-source (skbuff.c refcount ops); linux-networking-docs
  (ownership handoff); linux-napi-docs; cwe (CWE-415, CWE-416).

## 10. `kfree_skb` vs `consume_skb`

- **RULE**: both release one reference and free the data when the last
  reference drops. `kfree_skb` is for error paths (drops are traceable as
  kfree_skb events and charge the drop reason); `consume_skb` is for the
  success path where the packet was consumed. They have the same refcount
  behavior, different drop accounting.
- **WHY AI GETS IT WRONG**: agents call `kfree_skb` on every path and lose the
  consumed/dropped distinction, or free "the skb" without understanding that
  the call only drops one reference.
- **CORRECT REASONING**: pick by intent: `consume_skb(skb)` on successful
  delivery, `kfree_skb(skb)` (with `SKB_DROP_REASON_*`) on error. Both
  decrement `skb->users`; the buffer is reclaimed only when the count
  reaches zero, so clones must each be released individually.
- **EXAMPLE** (bad):
  ```c
  if (err) kfree_skb(skb);       /* ok */
  else     kfree_skb(skb);       /* should be consume_skb; accounting skew */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (err) kfree_skb_reason(skb, SKB_DROP_REASON_DEV_QUEUE_FULL);
  else     consume_skb(skb);
  ```
- **VERIFICATION**: review each free site against the path (error vs success);
  harness asserts refcount decrements, never below zero.
- **SOURCE**: kernel-source (skbuff.c `kfree_skb`/`consume_skb`);
  linux-networking-docs; cwe (CWE-415).

## 11. Linear vs paged data (`frags`)

- **RULE**: `skb_put` / `skb_push` / `skb_pull` operate only on the linear
  area `[data, tail)`. Data beyond the linear area lives in page fragments
  (`skb_shinfo(skb)->frags`) tracked with `skb_frag_*` helpers and
  `skb->data_len`. Do not `skb_put` into frags or access frags through
  `skb->data`.
- **WHY AI GETS IT WRONG**: agents treat `skb->len` as one contiguous region
  and index `skb->data[i]` across the whole length, reading garbage where
  the pages begin.
- **CORRECT REASONING**: `skb->len = skb_headlen() + skb->data_len`. The
  linear head is contiguous; frags require `skb_frag_address`/kmap or
  `skb_copy_bits` to read. Length checks must account for both parts.
- **EXAMPLE** (bad):
  ```c
  for (i = 0; i < skb->len; i++)
      process(skb->data[i]);     /* i beyond headlen -> past the linear area */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  unsigned int lin = skb_headlen(skb);   /* linear part only */
  for (i = 0; i < lin; i++)
      process(skb->data[i]);
  skb_copy_bits(skb, lin, buf, skb->len - lin);  /* frags via helper */
  ```
- **VERIFICATION**: harness limits `skb_put_emu` to the linear buffer; review
  checks that frag access uses the frag API.
- **SOURCE**: linux-skbuff-docs (skb_shinfo/frags); kernel-source;
  linux-networking-docs; cwe (CWE-787).

## 12. Reading and writing protocol headers in place

- **RULE**: headers are read/written at `skb->data` (after any `skb_pull`)
  using alignment-safe accessors (`get_unaligned`, `put_unaligned` and
  `cpu_to_*` / `ntoh*` conversions). Never cast `skb->data` to a packed
  struct pointer on architectures with strict alignment, and never trust
  header fields as lengths without validation.
- **WHY AI GETS IT WRONG**: casts `struct ethhdr *h = (void *)skb->data;`
  and reads field lengths straight into `skb_put`/`skb_pull` sizes.
- **CORRECT REASONING**: header layouts are packed and byte-ordered per
  protocol; use explicit offset/unaligned access and convert with
  `ntohs`/`ntohl`. A length read from the header is untrusted input — bound
  it before it reaches any pointer arithmetic.
- **EXAMPLE** (bad):
  ```c
  unsigned int n = ((struct tcphdr *)skb->data)->doff * 4;
  skb_pull(skb, n);        /* n can exceed skb->len -> data past tail */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  unsigned int n = ((struct tcphdr *)skb->data)->doff * 4;
  if (n < 20 || n > skb->len)
      return -EINVAL;
  skb_pull(skb, n);
  ```
- **VERIFICATION**: harness: oversized pull hits the BUG diagnostic and
  returns NULL; review flags unvalidated header-derived lengths.
- **SOURCE**: linux-networking-docs (header access conventions); kernel-source;
  cwe (CWE-190, CWE-787).

## Quick detection table

| Pattern | Class | Check |
|---|---|---|
| `skb_put` past tailroom | CWE-787 | `len <= skb_tailroom()` |
| `skb_push` without reserve | CWE-787 | `len <= skb_headroom()` |
| stale `skb->data` after pull/push | CWE-787/416 | re-read after move |
| write into shared clone data | CWE-787 | `skb_copy` before data writes |
| two owners free one skb | CWE-415 | track `skb->users`/ownership |
| `kfree_skb` on success path | accounting | use `consume_skb` |
| unchecked header length | CWE-190/787 | validate before put/pull |
| indexing past headlen | CWE-787 | `skb_copy_bits` for frags |
| double free of shared skb | CWE-415 | one free per holder |
