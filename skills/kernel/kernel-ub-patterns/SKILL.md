---
name: kernel-ub-patterns
description: Use when reviewing or writing Linux-kernel-style C code for UB: strict aliasing in kernel code, container_of misuse, __user annotation violations, and integer overflow in kernel APIs. Teaches the kernel-specific instances of undefined behavior that differ from userspace, and how to reason about them against the C standard and kernel conventions.
---
# Kernel-Specific UB Patterns

## When to use

- Reviewing kernel/driver C code for strict-aliasing violations
  (type-punning through wrong pointers, `union` vs `memcpy`).
- Verifying `container_of` usage: is the pointer really the member of the
  containing struct, and is the type/offset right?
- Auditing `__user` annotation discipline: kernel pointers vs user
  pointers, `copy_to_user`/`copy_from_user` argument contracts, sparse
  warnings.
- Checking integer overflow in kernel APIs: `kmalloc(size * count)`,
  size_t arithmetic, refcounts, offsets, `check_add_overflow` /
  `check_mul_overflow` patterns.
- Extending kernel-adjacent code with C that must satisfy both the
  standard's UB rules and kernel coding style.

## When not to use

- General C UB taxonomy (signed overflow, null deref, etc.) — use
  `c-undefined-behavior` and `compiler-ub-assumptions`.
- Userspace-only UB patterns (no kernel API involved) — different skill
  set.
- Rust-specific UB (`unsafe`, aliasing, panic) — use
  `rust-unsafe-reasoning`.
- Locking/memory-ordering bugs — use `kernel-rcu-memory-barriers` and
  `kernel-atomic-context`.

## What the agent often gets wrong

- Writing type-punning via a cast (`(uint32_t *)buf`) and reading, which
  violates strict aliasing (C11 6.5p7) — the compiler may reorder/merge
  accesses, silently corrupting kernel data. The correct pattern is
  `memcpy` or a `union` (for same-layout reinterprets) or `get_unaligned`.
- Misusing `container_of`: passing a pointer that is not actually the
  member (e.g. from a different embedded struct), or the wrong member
  name/type — the computed offset points into the middle of a struct.
  `container_of` cannot verify this at compile time; the agent must trace
  where the pointer came from (e.g. `list_for_each_entry` guarantees the
  list-head relationship, a bare `container_of` on a random pointer does
  not).
- Treating `__user` as a runtime qualifier: `__user` is a sparse
  annotation, not a compiler check. Mixing kernel and user pointers in
  `copy_from_user`/`copy_to_user` or dereferencing a `__user` pointer
  directly produces sparse warnings and real memory-safety bugs
  (accessing user memory via kernel addresses).
- Overflow in size computations: `kmalloc(n * sizeof(*x))` wraps for
  large `n` (32-bit size_t on many kernels) → undersized allocation →
  heap overflow. Missing `check_mul_overflow`, `array_size`,
  `struct_size`, `size_mul` helpers.
- Signed integer overflow in kernel code: the kernel builds with
  `-fwrapv` (defined wrap), so signed overflow is *not* UB in the kernel
  build — but the agent either assumes UB (and "fixes" it with undefined
  behavior) or ignores wrap entirely where it still causes bugs (size
  arithmetic, refcounts).
- Forgetting that `-fno-strict-aliasing` is NOT a license to type-pun
  freely: kernel code still targets correctness under the standard, and
  relying on the flag hides aliasing bugs from sanitizers.

## How to reason correctly

1. **Strict aliasing**: to read a byte buffer as a struct, use `memcpy`
   or `get_unaligned`/`put_unaligned`; to reinterpret between
   same-size types, use a `union` only where the standard permits
   (or `memcpy` always). Never `*(T *)p` on a `char[]` buffer that the
   compiler may treat as another type.
2. **container_of**: verify provenance — the pointer must have been
   derived from the member (list/htable macros guarantee it); check the
   member name against the struct definition; if the member is at a
   nonzero offset, a wrong name changes the offset silently. When in
   doubt, write the offset check as a compile-time assert
   (`offsetof(type, member)`).
3. **__user**: `copy_from_user`/`copy_to_user` take `__user`-annotated
   pointers and return bytes-not-copied; never dereference a `__user`
   pointer in kernel space, and always check the return value. Run
   sparse (`make C=1 CHECK=sparse`) to catch annotation mismatches.
4. **Integer overflow**: compute sizes with the kernel helpers
   (`array_size`, `struct_size`, `size_mul`, `check_mul_overflow`,
   `check_add_overflow`, `check_sub_overflow`); never compute
   `count * size` bare in a size_t context. Validate `count` against a
   maximum before allocation. For refcounts use `refcount_t`, not
   `atomic_t` for lifetimes.
5. **Know the build flags**: the kernel compiles with `-fwrapv`
   (signed wrap defined) and `-fno-strict-aliasing` — but treat those as
   *relief from miscompilation*, not as permission to write code that
   relies on wrap or punning; keep the code standard-correct so
   sanitizers and other builds (modules, out-of-tree) stay correct.

## What to verify

- No `*(T *)p` type-punning on buffers; all reinterpretation via
  `memcpy`/`get_unaligned`/union where legal.
- Every `container_of` call's pointer provenance is traced; member name
  matches the struct definition; offsets sanity-checked.
- All `copy_to_user`/`copy_from_user` pointers carry `__user` and
  returns are checked; no direct dereference of `__user` pointers.
- All size computations use `array_size`/`struct_size`/`size_mul` or an
  explicit `check_mul_overflow`; no bare `n * sizeof` in size_t context.
- `-Wall -Wextra -Werror` compiles the fixture clean; `-fsanitize=undefined`
  reports nothing; sparse (documented) reports no annotation mismatches.

## How to verify

```
# Host-verifiable: strict aliasing + container_of + overflow (gcc 16.1)
gcc -Wall -Wextra -Werror -O2 examples/good/kernel_ub_good.c -o ubgood
ubgood                          # PASS: aliasing-safe, container_of correct
gcc -Wall -Wextra -Werror -O2 -fsanitize=undefined -c examples/good/kernel_ub_good.c
#   ^ compile-only UBSan instrumentation check (executed; the link-time
#   libubsan runtime is not installed on this host — the run is -O2)

gcc -Wall -Wextra -Werror -O2 examples/bad/kernel_ub_bad.c -o ubbad
ubbad                           # BAD: aliased read — the "looks right" case

gcc -O2 examples/bad/strict_aliasing_violation.c -o alibad
gcc -O2 -fno-strict-aliasing examples/bad/strict_aliasing_violation.c -o alibad2
alibad ; alibad2               # BAD: results differ (0 vs 1374389535)

gcc -Wall -Wextra -O2 -Wno-array-bounds examples/bad/container_of_misuse.c -o containerbad
containerbad                    # BAD: bogus container_of base, no diagnostic
gcc -Wall -Wextra -Werror -O2 examples/bad/user_pointer_and_size.c -o ubsz
ubsz                            # BAD: undersized allocation from wrapped size

# Sparse / __user audit (documented; sparse not installed on this host):
make C=1 CHECK=sparse            # kernel build-time annotation check
sparse file.c                    # standalone annotation audit
```

The aliasing, container_of, and overflow fixtures execute on this host
(gcc 16.1.0; output in `evals/README.md`); the sparse/`__user` runs are
documented, UNVERIFIED here.

## Where the knowledge comes from

- `iso-c11-n1570` — strict aliasing (6.5p7), union reinterprets.
- `kernel-coding-style` — kernel C conventions.
- `kernel-source` — `container_of`, `__user`, `copy_to_user`,
  `array_size`/`struct_size` helpers, `-fwrapv`/`-fno-strict-aliasing`
  build flags.
- `kernel-driver-api` — copy_to/from_user contracts.
- `cert-c` — aliasing and integer-overflow rules (EXP36-C, INT32-C).

## Related skills

- `c-undefined-behavior` (require; general UB taxonomy).
- `kernel-uaccess-safety` (require; __user/copy_to_user discipline).
- `compiler-ub-assumptions` (recommend; why the compiler exploits UB).
- `kernel-atomic-context` (recommend; refcount_t/atomic_t in context).
- `kernel-driver-char-device-lifecycle` (recommend; where these patterns
  appear in real drivers).

## Evaluation

- Synthetic: `bad/kernel_ub_bad.c` (misaligned/aliased read) and
  `bad/strict_aliasing_violation.c` (wrong value from alias) must be
  flagged; `good/kernel_ub_good.c` must pass clean under
  `-fsanitize=undefined`.
- False-positive: `memcpy`-based reinterpretation is CORRECT (not
  flagged); `container_of` inside `list_for_each_entry` is correct;
  signed wrap in kernel code (with `-fwrapv`) is defined, not UB.
- Historical: kernel aliasing/overflow bugs (e.g. size-wraparound in
  drivers, refcount-overflow class) are documented in kernel/CVE lore;
  UNVERIFIED as named incidents on this host.
- Adversarial: code that "works" because `-fno-strict-aliasing` is on
  but is UB under the standard (breaks sanitizer/module builds); a
  `container_of` on a wrong-member pointer that looks plausible.
- Verified facts and commands: `evals/README.md`.
