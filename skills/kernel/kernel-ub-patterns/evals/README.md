# Evaluation — kernel-ub-patterns

Skill: `skills/kernel/kernel-ub-patterns`. Stability target: `evaluated`.
C-UB semantics KNOWN from iso-c11-n1570; kernel conventions KNOWN from
kernel-source / kernel-coding-style / kernel-driver-api. Aliasing,
container_of, and overflow fixtures EXECUTED on this host (gcc 16.1.0);
sparse and real kernel-build runs UNVERIFIED.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/strict_aliasing_violation.c` | aliased uint32 read of uint8 buffer → flagged | executable |
| easy/negative | `bad/kernel_ub_bad.c` | *(uint32_t*)p aliased read → UBSan flags | executable |
| medium/negative | `bad/container_of_misuse.c` | container_of on non-member pointer → flagged | executable |
| medium/negative | `bad/user_pointer_and_size.c` | user deref + wrapped size → flagged | executable |
| easy/positive | `good/kernel_ub_good.c` | memcpy aliasing, proven container_of, overflow-checked → PASS | executable |

Detection rule: (1) no type-punning reads of buffers; (2) container_of
provenance traced + member matches; (3) __user via copy helpers with
checked returns; (4) sizes via array_size/struct_size/check_mul_overflow;
(5) -fwrapv/-fno-strict-aliasing understood as relief, not license.

## False-positive evals (correct code must NOT be flagged)

- `memcpy`-based reinterpretation — correct under 6.5p7, never flagged.
- `container_of` inside `list_for_each_entry` (proven member
  relationship) — correct.
- Signed wrap in kernel code compiled with `-fwrapv` — defined behavior,
  not UB (do not "fix" it as UB).
- `union` reinterprets where the standard permits (same layout, active
  member discipline) — correct, distinct from punning.

## Historical evals

- Kernel driver size-wraparound bugs (`n * sizeof` in 32-bit size_t
  builds) and the refcount-overflow class are documented in kernel
  history and CVE lore. UNVERIFIED as named incidents on this host.
- Aliasing-dependent driver behavior that differed between -O0 and -O2
  is a documented debugging class in kernel communities. UNVERIFIED as a
  named incident here.

## Adversarial evals

- Code that "works" only because `-fno-strict-aliasing` is set: correct
  at -O0, wrong/UB under the standard or a sanitizer build — the
  unconditional-pass trap from `meta-verification-harness-validity`.
- A `container_of` on a wrong-member pointer that "looks plausible"
  (correct types, wrong struct) — only provenance tracing catches it.
- A size computation that wraps only for absurd `count` values that
  example-based tests never reach.
- An agent claiming "signed overflow is UB in the kernel" on `-fwrapv`
  builds — a false normative claim that corrupts the reasoning.

## Verification commands

```
gcc -Wall -Wextra -Werror -O2 examples/good/kernel_ub_good.c -o ubgood && ubgood
gcc -Wall -Wextra -Werror -O2 -fsanitize=undefined -c examples/good/kernel_ub_good.c
gcc -Wall -Wextra -Werror -O2 examples/bad/kernel_ub_bad.c -o ubbad && ubbad
gcc -O2 examples/bad/strict_aliasing_violation.c -o alibad && alibad
gcc -O2 -fno-strict-aliasing examples/bad/strict_aliasing_violation.c -o alibad2 && alibad2
gcc -Wall -Wextra -O2 -Wno-array-bounds examples/bad/container_of_misuse.c -o containerbad && containerbad
gcc -Wall -Wextra -Werror -O2 examples/bad/user_pointer_and_size.c -o ubsz && ubsz

# Sparse / __user audit (documented; sparse not installed on this host):
make C=1 CHECK=sparse            # kernel build-time annotation check
```

Note: libubsan is not installed on this host, so `-fsanitize=undefined`
runs at compile-only (-c); the good fixture is run at -O2 and PASSes.

## Verified facts

- KNOWN: strict-aliasing rule (6.5p7); container_of offset semantics and
  provenance requirement; __user copy-helper contract; kernel overflow
  helpers; kernel `-fwrapv`/`-fno-strict-aliasing` flags. Sources:
  iso-c11-n1570, kernel-source, kernel-coding-style, kernel-driver-api,
  cert-c.
- EXECUTED on this host: `ubgood` PASS; `-fsanitize=undefined -c`
  accepted; `ubbad` shows the "looks right" value; `strict_aliasing`
  divergence 0 vs 1374389535 (the -O2 / -fno-strict-aliasing pair);
  `containerbad` prints a bogus `fake->id`; `ubsz` prints the
  undersized-allocation result (recorded above).
- UNVERIFIED: sparse runs, real kernel build with `make C=1`, kernel
  runtime on hardware, libubsan-linked runs (runtime not installed).

## Scoring

- precision: every flagged issue maps to a reference rule (1–5).
- recall: all bad fixtures detected (aliasing, container_of, user deref,
  size wrap).
- FP-rate: memcpy/union/list-iteration/proven container_of patterns
  produce zero flags.
- Decisive test: "is this a legal lvalue access under 6.5p7?" and "can I
  trace the container_of pointer back to the member?"

### Executed output (2026-08-17, MSYS2 gcc 16.1.0)

```
$ gcc -Wall -Wextra -Werror -O2 examples/good/kernel_ub_good.c -o ubgood && ./ubgood
PASS: aliasing-safe, container_of correct, overflow-checked
exit 0

$ gcc -Wall -Wextra -Werror -O2 -fsanitize=undefined -c examples/good/kernel_ub_good.c
exit 0   (compile-only UBSan instrumentation accepted; libubsan absent)

$ gcc -Wall -Wextra -Werror -O2 examples/bad/kernel_ub_bad.c -o ubbad && ./ubbad
value=0x12345678
BUG hidden: -O0 looks right; UB flagged by UBSan
exit 0   (the "looks right" illusion — UBSan/compile-check required)

$ gcc -O2 examples/bad/strict_aliasing_violation.c -o alibad && ./alibad
result=0
exit 0

$ gcc -O2 -fno-strict-aliasing examples/bad/strict_aliasing_violation.c -o alibad2 && ./alibad2
result=1374389535
exit 0   (aliasing divergence demonstrated: 0 vs 1374389535)

$ gcc -Wall -Wextra -O2 -Wno-array-bounds examples/bad/container_of_misuse.c -o containerbad && ./containerbad
fake->id = 154 (should be unreachable in correct code)
exit 0   (bogus base, no diagnostic — flagged by review)

$ gcc -Wall -Wextra -Werror -O2 examples/bad/user_pointer_and_size.c -o ubsz && ./ubsz
BUG: undersized allocation accepted (wrapped size)
exit 0   (flagged by review; user deref is the sparse/__user class)
```
