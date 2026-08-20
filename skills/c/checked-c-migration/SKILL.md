---
name: checked-c-migration
description: Use when adding Checked C bounds annotations to legacy C, or when a memory-safety retrofit needs checked pointers, _Array_ptr, bounds declarations, or incremental migration. Teaches annotation patterns LLMs can propose and how to verify them, distinct from rewriting in Rust.
---

# Checked C Migration

## When to use

- Retrofitting spatial memory safety onto a legacy C codebase in place, without a rewrite.
- Choosing the checked pointer type and bounds declaration that fit a specific pointer:
  `_Ptr<T>`, `_Array_ptr<T> : count(n)`, `_Nt_array_ptr<T>`.
- Adding `count`/`byte_count`/`bounds(none)` declarations, `_Checked` regions/blocks, or
  `_Dynamic_check` runtime guards.
- Planning an incremental, function-by-function migration where checked and unchecked code
  must coexist and interoperate.
- Reading or driving the 3C inference tool (`3c.py`) and acting on its root-cause reports.
- Verifying that a proposed annotation set actually compiles under the Checked C compiler
  fork and that runtime bounds checks fire.

## When not to use

- Rewriting the code in Rust — use `rust-ffi-boundary` for the FFI seam instead.
- C++ where `std::span` / `std::string` or a sanitizer+`-D_FORTIFY_SOURCE` workflow is the
  target (Checked C is C-only).
- A one-off audit of a single string bug — route through `c-string-and-buffer-safety`.
- ASan/UBSan hardening with no type-level change — that keeps plain `char *` and does not
  need annotation inference.
- Formal proofs of the whole program — Checked C is compile-time + runtime enforcement, not
  a full proof system (see `meta-verification`).

## What the agent often gets wrong

1. Over-annotating: turning every pointer into `_Ptr<T>` when iteration or subscripting is
   happening. A `_Ptr` cannot be indexed or advanced; the correct annotation is
   `_Array_ptr<T>` with a bound, and the bounds must cover every access.
2. Bounds that do not match reality: `count(10)` on a buffer that is actually shorter, or
   `count` where `byte_count` is needed (or vice versa). The compiler rejects the mismatch
   only if it can prove it; otherwise it becomes a wrong runtime check or an insertion gap.
3. Forgetting null-termination: annotating `char *` as `_Nt_array_ptr` without proving the
   data is NUL-terminated. `_Nt_array_ptr` derives its bounds from `strlen+1`; unproven
   termination makes the bounds unsound.
4. Casting away the checker: inserting `(_Ptr<char>)` / unchecked casts to silence errors
   instead of fixing the bounds. Casts erase the guarantee precisely where it matters.
5. Assuming checked code is fully safe end-to-end: the guarantee is spatial safety inside
   checked regions. Unchecked regions still have holes, and unchecked-to-checked interop
   boundaries must be reasoned about explicitly.
6. Missing `_Dynamic_check` where bounds depend on runtime values that cannot be proven
   statically (e.g. a length read from a file header). Static bounds alone do not cover this.
7. Claiming the migration compiles under stock gcc. Checked C keywords are accepted only by
   the Checked C clang fork (`clang -fcheckedc-extension`); stock gcc rejects them.

## How to reason correctly

1. Work per function, starting with the smallest annotation change that keeps the code
   compiling and behavior identical. Migrate incrementally; do not annotate a whole module
   in one pass.
2. Classify each pointer by its use: single object (no arithmetic, no subscript beyond `[0]`)
   → `_Ptr<T>`; counted contiguous array → `_Array_ptr<T> : count(n)`; null-terminated byte
   array → `_Nt_array_ptr<T>` (termination must be provable).
3. Derive bounds from the actual API contract: the caller's stated count, `strlen+1` for
   strings, `sizeof`/element-size arithmetic for loops, the capacity recorded at allocation.
   Use `byte_count(n)` only when the bound is expressed in bytes and the element size is
   honored (n must be a multiple of the element size for indexing).
4. Where a bound cannot be proven, either widen it with `_Dynamic_check` at the use site or
   refactor the root cause (thread the size as a parameter, store capacity next to data).
   Never blind-cast to silence the checker.
5. Verify with the Checked C compiler after every step; unchecked ↔ checked interop happens
   at function boundaries, where checked pointers keep their representation and unchecked
   callers can still pass ordinary pointers.
6. Treat every LLM-proposed annotation as a hypothesis to be compiler-checked and
   runtime-probed, not as a finished answer.

## What to verify

- Every checked pointer carries a bounds declaration that is consistent with all of its uses
  (indices < count, copy lengths <= count, string uses inside the null-terminated region).
- The code compiles clean with the Checked C clang fork at `-Wall -Wextra`
  (`clang -fcheckedc-extension`); stock-gcc builds are for the plain-C control examples only.
- A runtime test that actually triggers an out-of-bounds access under the Checked C compiler
  shows the bounds check firing (trap/abort), not silent corruption.
- No blind casts that suppress checks; every unchecked boundary (casts, calls into unchecked
  code) is documented and reasoned about.
- Unchecked ↔ checked interop conversions are only where the representation and lifetime
  actually justify them.

## How to verify

Host (this repo, no Checked C compiler needed):
```
python examples/tools/annotation_infer.py
# expect: every scenario's PASS/FAIL matches its expected label; final "MODEL SOUND"
gcc -Wall -Wextra -Werror -O2 examples/good/incremental_step.c -o incremental_step.exe
./incremental_step.exe
# expect: "incremental_step: all bounds respected", exit 0
gcc -Wall -Wextra -O2 examples/bad/bounds_lying.c -o bounds_lying.exe
./bounds_lying.exe
# expect: -Wstringop-overflow-style warning and a corrupted canary (real overflow)
```

Target (Checked C compiler):
```
clang -fcheckedc-extension -Wall -Wextra -Werror -o out examples/good/annotated_checkedc.c
clang -fcheckedc-extension -Wall -Wextra -Werror -DOOB_PROBE -o out_probe examples/good/annotated_checkedc.c
./out_probe
# expect: runtime trap on the deliberate out-of-bounds write (pc[3] with count(2))
```

3C tool flow (where available):
```
python3 3c/src/3c.py file.c        # inference + root-cause report
# iterate: fix root causes, re-run, then verify with clang -fcheckedc-extension
```

## Where the knowledge comes from

- Checked C specification (https://github.com/microsoft/checkedc/blob/main/spec/checkedc.md)
- C to Checked C by 3C — arXiv 2203.13445 (https://arxiv.org/abs/2203.13445)
- A Formal Model of Checked C — arXiv 2201.13394 (https://arxiv.org/abs/2201.13394)
- Checked C clang fork (https://github.com/microsoft/checkedc-clang)
- Microsoft Research on LLM-assisted migration to memory-safe C dialects

## Related skills

- `c-undefined-behavior` — the out-of-bounds accesses Checked C annotations prevent are UB.
- `c-string-and-buffer-safety` — string bounds are the hardest annotations; that skill covers
  the raw-C reasoning underneath.
- `c-integer-promotion-and-conversion` — count/byte_count arithmetic and `sizeof` must not
  overflow or truncate.
- `ffi-boundary-cross-language` — checked-to-unchecked and cross-language interop seams.
- `rust-ffi-boundary` — the alternative memory-safe endpoint: FFI from checked C to Rust.
- `capability-based-security` — bounds as capabilities; the mental model transfers.
- `meta-verification` — how to structure the host/target verification split used here.

## Evaluation

Synthetic: the six IR scenarios in `examples/tools/annotation_infer.py` (single object,
correct count, count too small, copy overflow, proven and unproven null-termination, unprovable
bounds fallback). False-positive: correctly bounded code must stay PASS — `_Ptr` on a single
object must not be escalated, and a proven `_Nt_array_ptr` must not be flagged. Historical:
classic spatial-safety CVEs (Heartbleed CVE-2014-0160, nginx CVE-2021-23017, curl CVE-2023-38545)
plus the 3C evaluation on 11 programs / ~319 KLOC from arXiv 2203.13445. Adversarial:
`count` vs `byte_count` mismatch, off-by-one at `index == count`, `p++` arithmetic in loops,
and unchecked interop boundaries. Full matrix and target commands in `evals/README.md`.
