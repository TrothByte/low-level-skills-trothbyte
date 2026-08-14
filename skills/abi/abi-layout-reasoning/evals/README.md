# Evaluation — abi-layout-reasoning

Skill: `skills/abi/abi-layout-reasoning`. Stability target: `evaluated`.

## Synthetic evals

- **easy/negative**: `struct { char c; int i; }` — compute size/offsets. Expected: size 8, off_i 4.
- **medium/negative**: `struct { char a; double d; char b; }` — expected size 24; reordered → 16.
- **hard/negative**: classify `struct { double d1, d2; }` (16 bytes, SSE) vs `struct { int a, b, c, d; }`
  (16 bytes, INTEGER) on SysV — registers vs stack/SSE.
- **adversarial (AD-06)**: a `repr(C)` struct at an FFI boundary whose padding the agent
  hand-sums wrong — must compute via `offsetof`, not intuition.

## Adversarial cross-ABI eval

- **AD-05 (works x86 fails ARM)**: packed struct field access — must flag unaligned access
  and use `memcpy` instead of a direct load.
- **Windows vs SysV**: same small-struct source, two prologues — agent must identify the
  ABI (Windows x64 packs 8-byte struct into `%rcx`; SysV uses `%edi,%esi`).

## False-positive evals

- A correct, verified struct layout (matching `offsetof`) — must NOT be flagged.
- A large struct deliberately passed by const pointer — must NOT be "corrected" to by-value.

## Verified facts (GCC 16.1, MinGW x64)

- `S1 {char c; int i;}` → size 8, align 4, off_i 4. `S2 {double d; char a; char b;}` → 16.
  `S3 {char a; double d; char b;}` → 24.
- `add_small(struct{int,int})` → single `%rcx` register on Windows x64 (packed); two registers
  on SysV/Linux.
- `f_big(struct{5×long})` → passed on stack (MEMORY class).

## Verification commands

```
gcc layout.c -o /tmp/layout && /tmp/layout   # offsetof/sizeof
gcc -O0 -S small.c && gcc -O0 -S big.c       # prologue/classification
```
