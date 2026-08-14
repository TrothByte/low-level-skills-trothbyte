# Evaluation — cache-and-numa-optimization

Skill: `skills/performance/cache-and-numa-optimization`. Stability target:
`evaluated`. Source files: `examples/good/*.c`, `examples/bad/*.c`.

## Host and timing

Verification host: Windows, GCC 16.1.0 (MSYS2 MinGW, x86-64), AMD Ryzen 5
5600H (6 cores / 12 threads, Zen 3, 16 MB L3). Timing uses
`QueryPerformanceCounter` (10 MHz counter on this host, ~0.1 us resolution).
Run each binary 3 times; best times are reported. Best-of-3 mitigates
scheduler and turbo jitter, which is visible in the raw runs below.

`perf` and `numactl` do not exist on Windows; all NUMA/pref counter claims
in this skill are documented as **Linux targets** (perf-wiki, numactl(8)),
not executed here.

## Verified facts (OBSERVED, GCC 16.1.0, MinGW x86-64, Windows)

All three pairs share identical worker code and only differ in layout or
loop order; the printed sums match within each pair (correctness preserved).

| Fact | Evidence (best of 3) |
|---|---|
| false sharing: adjacent int counters slow down | `false_sharing.c`: 88.0 ms (runs 92.3/88.0/96.5) |
| padding to one line per counter removes it | `padded_counters.c`: 5.3 ms (runs 5.3/5.3/5.6), `sizeof(counter)=64`, pinned to 2 physical cores |
| false-sharing slowdown factor | ~16x (88.0 / 5.3) |
| column-major traversal of row-major matrix is slow | `strided_access.c`: 117.3 ms (runs 118.3/117.3/118.0) |
| row-major traversal of the same matrix is fast | `row_major.c`: 12.9 ms (runs 13.8/12.9/13.0) |
| locality slowdown factor | ~9x (117.3 / 12.9), identical sums (68702699520.0) |
| AoS with padding/untouched fields wastes bandwidth | `array_of_structs.c`: 9.5 ms (runs 10.3/12.2/9.5), `sizeof(struct Particle)=24` |
| SoA of the same data is faster | `struct_of_arrays.c`: 6.2 ms (runs 6.2/6.8/7.0) |
| AoS/SoA factor | ~1.5x, identical sums (31999996000000.0) |
| `_Alignas(64)` on a member sets both alignment and sizeof | `sizeof(struct padded_counter) == 64` (compile-time check in run output) |

A measurement caveat worth recording: with a `volatile double` accumulator
the AoS/SoA and strided/row-major differences shrink drastically (2.2x, 1.0x)
because the volatile read-modify-write chain serializes both variants. The
shipped examples therefore use a register accumulator, which isolates the
memory-access pattern. The false-sharing pair MUST keep `volatile` counters:
without it the compiler hoists the store out of the loop.

## Verification commands and actual output

```
# False sharing pair
gcc -O2 -pthread examples/bad/false_sharing.c -o fs_bad.exe && ./fs_bad.exe
gcc -O2 -pthread examples/good/padded_counters.c -o fs_good.exe && ./fs_good.exe

# Locality pair (same 4096x4096 matrix)
gcc -O2 examples/bad/strided_access.c -o strided.exe && ./strided.exe
gcc -O2 examples/good/row_major.c -o rowmajor.exe && ./rowmajor.exe

# Layout pair (same 8M elements)
gcc -O2 examples/bad/array_of_structs.c -o aos.exe && ./aos.exe
gcc -O2 examples/good/struct_of_arrays.c -o soa.exe && ./soa.exe
```

Observed output (run 1 of 3):

```
false sharing: counters {20000000, 20000000}, pinned=yes
elapsed 92.290 ms (20000000 iterations/thread)
padded counters: {20000000, 20000000}, pinned=yes, sizeof(counter)=64
elapsed 5.300 ms (20000000 iterations/thread)
column-major sum 68702699520.0
elapsed 118.304 ms (4096x4096 doubles)
row-major sum 68702699520.0
elapsed 13.831 ms (4096x4096 doubles)
AoS sum 31999996000000.0 (sizeof(struct Particle) = 24)
elapsed 10.276 ms (8000000 elements)
SoA sum 31999996000000.0
elapsed 6.152 ms (8000000 elements)
```

## Synthetic evals

- **easy**: two threads increment their own element of `volatile unsigned
  counters[2]` — expected answer: false sharing; the two ints share one
  64-byte line; fix = one cache line per counter (`_Alignas(64)` struct,
  sizeof 64) or per-thread buffers.
- **medium**: `for (j) for (i) sum += m[i][j];` over `m[ROWS][COLS]` —
  expected answer: column-major access to row-major memory; inner loop must
  advance the contiguous (last) index; measured ~9x on this host.
- **medium**: sum only `parts[i].x` of `struct Particle { double x,y; int
  id; }` (sizeof 24) — expected answer: AoS padding/untouched-field waste;
  SoA is denser; measured ~1.5x on this host.
- **hard**: `_Alignas(64) unsigned counters[2];` presented as a false-sharing
  fix — expected answer: alignment alone is not enough; two 4-byte objects
  still share one line; alignment AND a 64-byte stride are both required.
- **hard**: linked-list traversal where the next node's address depends on
  the current node's contents — expected answer: hardware prefetchers cannot
  predict it; this is a legitimate case for software prefetch, unlike
  sequential streaming.

## Adversarial evals

- `__builtin_prefetch` added to a plain sequential `sum += a[i]` loop — agent
  must predict no gain (hardware prefetcher already ahead) and demand a
  before/after measurement instead of prescribing prefetch.
- "`numactl --cpunodebind=0 ./prog` runs entirely on node 0 including
  memory" — agent must reject: CPU binding and memory policy are separate;
  first-touch decides pages unless `--membind` is given.
- "Struct padding is a compiler bug" — agent must classify padding as
  implementation-defined ABI layout, a bandwidth concern only in hot loops.
- "SoA is always faster, rewrite the whole codebase" — agent must check the
  loop: if all fields are consumed, AoS is at least as good.
- "Add `volatile` to fix the slow multithreaded counter" — agent must reject;
  volatile does not touch coherency traffic and may even force extra traffic.

## False-positive evals (must NOT flag)

- row-major loops over row-major arrays — correct and cache-friendly; must
  not be flagged as "unoptimized".
- SoA layouts and per-thread padded counters — valid fixes; must not be
  flagged as padding waste.
- `_Alignas(64)` with sizeof < 64 on a SINGLE-threaded object — alignment is
  legitimate; false sharing requires two writers to the same line, so it is
  not a defect here.
- an AoS loop that reads all fields (dense per-element use) — correct; SoA
  is not universally better.
- struct padding in cold/unmeasured code — a non-issue until a hot loop is
  shown to be bandwidth-bound; flagging it without measurement is a false
  positive.
- `volatile` counters in the false-sharing benchmark — intended (prevents
  store hoisting); not a defect.

## Scoring

- detection: names the real mechanism (false sharing / strided access /
  padding waste) from the access pattern, not from a guess about "slow
  code".
- reasoning: predicts which of the two layouts is faster and why BEFORE
  running.
- fix: changes layout/order/padding (not flags, not `volatile`, not blanket
  prefetch) and keeps results identical.
- verification: demonstrates with best-of-3 wall times from the paired
  binaries, not with "it compiled".

## Sources exercised

`intel-opt-manual`, `agner-fog`, `perf-wiki`, `iso-c11-n1570` (§6.2.8,
§7.22.3.1), `gcc-manual` (`__builtin_prefetch`), plus `numactl(8)` as a
documented Linux target not present in the sources registry. Registry ids
per `registry/sources.yaml`; full reasoning in `references/cache-numa.md`.
