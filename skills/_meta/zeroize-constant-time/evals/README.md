# Evaluation — zeroize-constant-time

Skill: `skills/_meta/zeroize-constant-time`. Stability target: `evaluated`.

## Verified facts (empirical, GCC 16.1, x86-64 MinGW, `gcc -O2 -S`)

| Claim | Evidence | Result |
|---|---|---|
| stack-local `memset(s,0,n)` before return is elided | `wipe_stack_buf` body | asm body = `ret` only; no fill, no memset |
| wipe through a caller-visible pointer is kept | `wipe_heap_buf` | asm `jmp memset` (tail call kept) |
| volatile-sink loop is NOT elided | `wipe_volatile_sink` | real `movb $0, (%rax)` store loop |
| volatile array + cast to `unsigned char*` wipes are elided | `login_check_good` (bad variant) | no wipe stores at -O2 |
| `secure_zero_memory` after last use survives | `login_check_good` (good variant) | store loop present after `verify_key` call |
| early-exit compare leaks position | `ct_bad` | asm `cmpb ... je` branch on secret bytes |
| accumulator compare is branchless per-byte | `ct_good` | vectorized `pxor/por`, single `testb/sete` |
| trivial `secret == guess` may fold to `sete` | `secret_branch` | `cmpl; sete` (branchless — but not guaranteed) |
| `table[secret]` is a cache-timing channel | `secret_index` | indexed load `movl (%rcx,%rdx,4), %eax` |
| `-fno-lifetime-dse` does NOT save a provably-dead local wipe | `wipe_stack_buf` | still `ret` only |

## Synthetic evals

- **easy/negative**: `wipe_stack_buf` — agent must say the wipe is removed at `-O2`, and
  prove it with `gcc -O2 -S` + grep.
- **easy/positive**: `secure_zero_memory` on a heap block before `free` — must NOT be flagged.
- **medium/negative**: `ct_memcmp_bad` (early-exit compare on secrets) — must be flagged as
  a timing leak; the fix is the accumulator pattern.
- **medium/positive**: `ct_memcmp_good` — must NOT be flagged; per-byte work is
  data-independent.
- **hard/negative**: volatile array + cast-to-nonvolatile wipe loop — compiles and "looks
  wiped", but the wipe is elided. Agent must catch the dropped qualifier and verify in asm.
- **hard/positive**: `ct_select_good` (bitwise select) — must NOT be flagged.
- **adversarial**: code that passes a functional test (the secret IS zeroed when run under
  a debugger/`-O0`) but leaks under `-O2`: same source, `-O0` keeps `call memset`, `-O2`
  deletes it. Agent must not conclude "wiped" from `-O0`.

## False-positive evals (correct code must not be flagged)

- Plain `memset(buf, 0, n)` on a PUBLIC, non-secret buffer whose result is unused — do NOT
  flag; no secret is at risk.
- `memcmp(a, b, n)` on public data (e.g. comparing file signatures that are not secret) —
  do NOT flag.
- A correctly-written constant-time accumulator compare — do NOT flag "branch".
- A single final `diff == 0` test after the accumulator loop — this branch is on the
  aggregate result (equal/not-equal), not on a secret byte position; do NOT flag.
- `secure_zero_memory` followed by `free` on the same block — correct wipe-then-free; do
  NOT flag.

## Verification commands

```
gcc -O2 -S examples/bad/zeroize_bad.c -o bad.s
# expect: wipe_stack_buf body is just `ret`; ct_memcmp_bad has cmpb/je
gcc -O2 -S examples/good/zeroize_good.c -o good.s
# expect: secure_zero_memory has a store loop; ct_memcmp_good has no early-exit je
# contrast:
gcc -O0 -S examples/bad/zeroize_bad.c -o bad_O0.s   # call memset survives at -O0
rg -n "memset|stosb|movb|je|jne|sete" good.s bad.s
```

## Scoring (for routing eval)

- precision: a flagged timing leak maps to a real construct (early exit, secret index,
  removed wipe) demonstrated in asm.
- recall: every bad snippet is detected; the elided-wipe cases are caught at `-O2`.
- FP-rate: public-data memcmp, correct accumulator compare, and wipe-then-free produce
  zero flags.
