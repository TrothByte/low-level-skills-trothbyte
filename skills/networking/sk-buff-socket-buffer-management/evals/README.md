# Evaluation — sk-buff-socket-buffer-management

Skill: `skills/networking/sk-buff-socket-buffer-management`. Stability
target: `evaluated`.

## Verified facts (host, this run)

- All examples compile clean with `gcc -Wall -Wextra -Werror -O2` using
  self-contained stubs (`examples/stubs.h`) — no kernel headers required.
  Toolchain: MinGW gcc 16.1.0 on Windows host.
- Good example runs with all assertions passing (exit 0) and prints
  `ALL CHECKS PASSED`.
- Bad example compiles and runs; each flaw is reproduced and prints its
  `BUG reproduced` diagnostic (exit 0) without crashing the harness.

| Example | Compile | Run | Observation |
|---|---|---|---|
| good/good_skb.c | 0 | 0 | "ALL CHECKS PASSED" — reserve/put/push/pull/clone/share-check/copy invariants asserted |
| bad/bad_skb.c | 0 | 0 | "BUG reproduced: skb_put exceeded tailroom" |
| bad/bad_skb.c | 0 | 0 | "BUG reproduced: modified shared skb data" |
| bad/bad_skb.c | 0 | 0 | "BUG reproduced: double free of a shared skb" |

The stub also prints its own diagnostics on each fault
(`BUG: skb_put exceeded tailroom (len 64, tailroom 32)` and
`BUG: double free of a shared skb`), so the class is detectable both by the
simulator and by the bad example's checks.

NOT verified on this host (documented targets, do NOT claim to have run):
kernel build with KASAN, QEMU boot, net selftests, syzkaller runs.

## Historical CVE evals (adversarial)

| CVE | Class | Fixture | Detect | Fix | Verify |
|---|---|---|---|---|---|
| CVE-2021-43267 | heap OOB write, CWE-787 | net/tipc/crypto.c `tipc_crypto_key_rcv()` | message length fields trusted, wrong length arithmetic precedes `skb_put`, writing past the allocation | validate the length against the buffer before `skb_put` | KASAN + TIPC reproducer |
| skb_put tailroom-overflow class | linear-area overflow, CWE-787 | any driver computing a `skb_put` length from untrusted/arith-wrapped input | `skb_put` never expands the buffer; write runs past `skb->tail` into tailroom/slab | `skb_tailroom()` check (or validated arithmetic) before `skb_put` | KASAN redzone fault |

Each eval: DETECT (find the missing bound) -> EXPLAIN (which sk_buff rule was
violated) -> FIX (add the validation) -> VERIFY (KASAN clean + reproducer).
Both entries describe only the documented classes (KNOWN, nvd-cve /
kernel-source); the exact TIPC exploit mechanics are not re-derived here.

## Synthetic evals

- easy/positive: alloc -> reserve -> put -> push -> pull sequence must NOT be
  flagged.
- easy/negative: `skb_put` with a length past the tailroom must be flagged.
- medium/negative: `skb_push` without a prior `skb_reserve` must be flagged.
- medium/negative: writing into the data area of a shared clone must be
  flagged.
- hard/negative: `skb_pull` with an unvalidated header-derived length must be
  flagged.
- hard/negative: two holders freeing the same skb (double free / wrong
  ownership) must be flagged.

## Adversarial evals

- Code that "works" in a single-owner smoke test but corrupts the payload
  once the skb is cloned (data-area write through a clone).
- Length arithmetic that passes a benign constant test but underflows or
  overflows with attacker-controlled packet fields before `skb_put`.
- A parser that caches `skb->data` before `skb_pull` and keeps parsing with
  the stale pointer — must be caught even though the bytes are still mapped.
- An `skb_share_check` user that writes data bytes believing the result is a
  private copy.

## False-positive evals (correct code must not be flagged)

- Correct reserve-then-put-then-push sequences with bounds asserted — do NOT
  flag.
- Modifying only the control block (`cb`) or struct fields of a clone — do
  NOT flag.
- `skb_copy` / `pskb_copy` used before data modification — do NOT flag.
- Single-owner frees and matched clone/free refcounts — do NOT flag.
- Paged-data reads via `skb_copy_bits` / frag helpers — do NOT flag.

## Verification commands

Host (self-contained stubs — recorded this run):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_skb.c -o /tmp/good_skb
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_skb.c -o /tmp/bad_skb
/tmp/good_skb     # -> ALL CHECKS PASSED, exit 0
/tmp/bad_skb      # -> 3x "BUG reproduced", exit 0
```

Target (kernel) — documented only, NOT run here:

```
# KASAN kernel build: skb_put tailroom overflow surfaces as a redzone fault
make defconfig && make -j$(nproc)   # CONFIG_KASAN=y
qemu-system-x86_64 -kernel arch/x86/boot/bzImage \
  -append "console=ttyS0 kasan=on" -nographic

# net selftests
make -C tools/testing/selftests TARGETS=net

# fuzz skb-heavy paths (TIPC, netfilter, drivers) with syzkaller
./bin/syz-manager -config manager.cfg   # CONFIG_KCOV + KASAN
```

## Scoring

- precision: every flagged pattern maps to a real sk_buff rule.
- recall: each bad snippet is detected.
- FP-rate: good snippets produce zero flags.
