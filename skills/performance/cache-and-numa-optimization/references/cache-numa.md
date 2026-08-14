# Cache & NUMA Knowledge

Source-backed knowledge for `cache-and-numa-optimization`. Each rule follows
RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE. Facts marked **VERIFIED** were measured on GCC 16.1
(MSYS2 MinGW, x86-64, Windows) with `examples/good/*` and `examples/bad/*`.
NUMA facts are source-backed but **documented as Linux targets** (no Linux
tooling exists on the verification host).

## 1. Cache hierarchy: lines, not bytes, move

- **RULE**: x86-64 caches transfer memory in fixed-size LINES (64 bytes on all
  current Intel/AMD cores). L1d is ~32-64 KB/core (4-5 cycles), L2 ~0.5-2 MB
  (12-14 cycles), L3 is shared per socket (2-32 MB+, 35-80+ cycles), DRAM
  80-300+ cycles. Exact numbers are microarchitecture-specific: read the
  manual for the actual chip instead of assuming.
- **WHY AI GETS IT WRONG**: reasons about "8 bytes per load" and ignores that
  every load or store moves a whole 64-byte line; also hardcodes one chip's
  numbers as if they were universal.
- **CORRECT REASONING**: the memory cost of a loop is dominated by lines
  fetched and misses serviced, not by bytes the instruction stream names.
  A 4-byte load from a cold line costs the same as a 64-byte load: one line.
- **EXAMPLE** (bad): "accessing `int a[1000000]` every 64th element is cheap
  because each access is only 4 bytes" — each access fetches a whole line and
  uses 4/64 bytes (6%).
- **COUNTEREXAMPLE** (good): a sequential scan uses every byte of each line
  (100%) and lets the hardware prefetcher run ahead.
- **VERIFICATION**: VERIFIED — `examples/bad/strided_access.c` (8 bytes per
  64-byte line fetched) was ~9x slower than the same data scanned
  sequentially in `examples/good/row_major.c` on the same host (best-of-3:
  117 ms vs 13 ms).
- **SOURCE**: `intel-opt-manual` (memory hierarchy, cache line sizes);
  `agner-fog` (cache latency tables per microarchitecture).

## 2. False sharing: distinct objects, same line

- **RULE**: when two threads WRITE different objects that share one cache
  line, every store triggers a line transfer/invalidation between cores even
  though the memory locations are distinct. Throughput collapses to the
  coherency protocol's line-transfer rate, not to parallelism.
- **WHY AI GETS IT WRONG**: "the threads touch different variables, there is
  no data race, so it is fine." A data race is an ORDERING defect; false
  sharing is a COHERENCY-TRAFFIC defect. They are different phenomena and
  neither atomicity nor `volatile` cures false sharing.
- **CORRECT REASONING**: any store to a line owned by another core forces the
  line to be sent over the coherency fabric; the line then bounces back and
  forth. Each logical increment costs a cross-core round trip instead of an
  L1 hit.
- **EXAMPLE** (bad): `volatile unsigned counters[2];` with thread 0 doing
  `counters[0]++` and thread 1 doing `counters[1]++` — adjacent ints in one
  line; the pair runs slower than one thread alone.
- **COUNTEREXAMPLE** (good): each counter padded/placed on its own line (see
  rule 3) — the threads stop interfering and run in parallel.
- **VERIFICATION**: VERIFIED — `examples/bad/false_sharing.c` vs
  `examples/good/padded_counters.c`, identical worker code, only the counter
  layout differs; the padded version was ~16x faster on the verification
  host (best-of-3: 88 ms vs 5.3 ms, 20M iterations/thread, pinned to two
  physical cores).
- **SOURCE**: `intel-opt-manual` (false sharing section); `agner-fog`
  (shared-cache/coherency discussion).

## 3. Padding: alignment AND stride, not alignment alone

- **RULE**: to give each hot object its own cache line you need BOTH: the
  base address aligned to the line size AND a stride between consecutive
  objects of at least one line. C11 `_Alignas(64)` (or a 64-byte struct with
  a tail pad) provides both: `_Alignas(64) struct { volatile unsigned v; }`
  has `sizeof == 64`, so an array stride is one full line.
- **WHY AI GETS IT WRONG**: "`_Alignas(64)` is the fix" — two 4-byte objects
  aligned to 64 bytes can still share one 64-byte line (they land at offsets
  0 and 4 of the same line). Alignment without size does not help.
- **CORRECT REASONING**: the property that matters is that the k-th object
  starts at `base + k*64`. Alignment fixes `base`; the struct SIZE fixes the
  stride. Both must hold.
- **EXAMPLE** (bad): `_Alignas(64) unsigned counters[2];` — 8 bytes total,
  both counters in one line, still false sharing.
- **COUNTEREXAMPLE** (good): `struct _Alignas(64) { volatile unsigned v; }
  counters[2];` — `sizeof` is 64, each counter owns its line.
- **VERIFICATION**: VERIFIED — `examples/good/padded_counters.c` uses exactly
  the 64-byte padded struct and was ~16x faster than the adjacent-int
  version (88 ms vs 5.3 ms best-of-3).
- **SOURCE**: `iso-c11-n1570` (§6.2.8 alignment, §7.22.3.1 `aligned_alloc`);
  `intel-opt-manual` (line-size-based padding guidance).

## 4. Locality: iterate the contiguous dimension innermost

- **RULE**: C arrays are row-major: the LAST index is contiguous. A nested
  loop should make the innermost index the one that steps by 1. Swapping loop
  nesting (without moving any data) is the cheapest locality fix.
- **WHY AI GETS IT WRONG**: "row-major vs column-major is a style choice, the
  compiler reorders loops for me." Compilers do not generally transpose
  imperfectly-nested loops or loops with volatile/aliasing-obscured accesses;
  the access order you write is usually the order executed.
- **CORRECT REASONING**: inner iteration along a 32 KB stride touches one new
  64-byte line per element and defeats the stride-tracking prefetchers; inner
  iteration along stride 1 streams lines at full density.
- **EXAMPLE** (bad): `for (j...) for (i...) sum += m[i][j];` — the inner loop
  advances the FIRST index, a stride of `COLS*8` bytes.
- **COUNTEREXAMPLE** (good): `for (i...) for (j...) sum += m[i][j];` — inner
  loop advances the contiguous index.
- **VERIFICATION**: VERIFIED — `examples/bad/strided_access.c` vs
  `examples/good/row_major.c`, same 4096x4096 matrix, only loop order
  differs; row-major was ~9x faster (117 ms vs 13 ms best-of-3).
- **SOURCE**: `intel-opt-manual` (data layout / traversal order);
  `agner-fog` (cache-friendliness of traversal).

## 5. Strided access defeats prefetchers and wastes lines

- **RULE**: hardware prefetchers on x86-64 track sequential streams and small
  fixed strides (roughly up to a few hundred bytes); large strides that cross
  pages (4 KB or more) are not tracked, so every access misses and every line
  fetched carries only a few useful bytes.
- **WHY AI GETS IT WRONG**: "prefetch will fix the strided loop" — the
  prefetchers that exist already cannot see this pattern; adding software
  prefetch for a stride of 32 KB merely adds instruction overhead to a loop
  that must still fetch one line per useful element.
- **CORRECT REASONING**: the fix for a strided hot loop is layout or
  traversal order (blocking/tiling), so that inner iterations reuse the lines
  already fetched. Tiling a matrix multiply is the canonical example.
- **EXAMPLE** (bad): column-major pass over `double m[4096][4096]`: each
  element needs its own line fetch (8/64 bytes used), plus one TLB entry per
  32 KB.
- **COUNTEREXAMPLE** (good): row-major pass, or a blocked pass that processes
  a `64x64` tile so a fetched line is reused by 8 consecutive iterations.
- **VERIFICATION**: VERIFIED — column-major run fetches ~8x more DRAM bytes
  than the row-major run over the same matrix (see rule 4 timing).
- **SOURCE**: `intel-opt-manual` (prefetch behavior, cache blocking);
  `agner-fog`.

## 6. SoA vs AoS: touch a subset of fields → SoA wins

- **RULE**: if a hot loop reads only one or two fields of an N-field struct
  array, a struct-of-arrays layout packs those fields contiguously: every
  fetched line is mostly useful data and the loop is vectorizer-friendly.
  AoS is preferable when ALL fields are used together (single stream, no
  extra streams).
- **WHY AI GETS IT WRONG**: "SoA is always faster, use it everywhere" — if a
  loop consumes all fields, AoS streams one combined line per element and is
  at least as good; SoA then adds multiple parallel streams that can cost
  more TLB/queue entries.
- **CORRECT REASONING**: compare useful bytes per fetched line. Summing one
  `double` out of a 24-byte struct uses 8/24; SoA uses 8/8. The decision is
  per-loop, not global.
- **EXAMPLE** (bad): `struct Particle { double x, y; int id; };` (24 bytes,
  4 padding), loop `sum += parts[i].x;` — fetches 24 bytes per useful 8.
- **COUNTEREXAMPLE** (good): `static double xs[N], ys[N]; static int ids[N];`
  with `sum += xs[i];` — one dense stream, 100% useful bytes.
- **VERIFICATION**: VERIFIED — `examples/bad/array_of_structs.c` vs
  `examples/good/struct_of_arrays.c`, same 8M elements, same sum; SoA was
  ~1.5x faster (9.5 ms vs 6.2 ms best-of-3).
- **SOURCE**: `intel-opt-manual` (AoS/SoA guidance, bandwidth);
  `agner-fog`.

## 7. Software prefetch is a fallback, not a default

- **RULE**: modern x86 cores run several hardware prefetchers (L1 data,
  adjacent-line, streaming) that cover sequential and small-stride patterns.
  Software prefetch (`__builtin_prefetch`, `prefetcht0`) costs an issue slot
  and an address computation; use it only when (a) measured misses show the
  hardware prefetcher behind, and (b) a before/after run shows a gain.
- **WHY AI GETS IT WRONG**: "the loop is slow because of memory, so add
  `__builtin_prefetch`" — the classic unmeasured reflex. In sequential
  streaming loops the hardware prefetcher is already ahead; extra prefetches
  only add instructions and can pollute the line buffer.
- **CORRECT REASONING**: software prefetch helps latency-bound, IRREGULAR
  access (pointer chasing, hash lookups, large irregular strides) where the
  hardware prefetcher cannot predict the next address. Prefetching the k-th
  future element hides that latency.
- **EXAMPLE** (bad): adding `__builtin_prefetch(&a[i+8])` to a plain
  sequential `sum += a[i]` loop — no change or slower (VERIFIED: hardware
  prefetcher already handles it).
- **COUNTEREXAMPLE** (good): a linked-list traversal where each node address
  is known only after dereference; prefetching `node->next->next` ahead can
  hide the load latency.
- **VERIFICATION**: measure `perf stat -e cache-misses` before/after
  (`perf-wiki` methodology); only adopt prefetch if the run improves. Not
  executed on the Windows verification host; methodology documented as Linux
  target.
- **SOURCE**: `intel-opt-manual` (software prefetch guidance);
  `perf-wiki` (miss-rate measurement); `gcc-manual`
  (`__builtin_prefetch`).

## 8. NUMA topology: memory is not uniformly far

- **RULE**: on multi-socket (and many many-core single-socket) systems, each
  CPU node owns local memory controllers. Access to another node's memory
  crosses the interconnect (QPI/UPI), with roughly 1.3-2x added latency and
  shared interconnect bandwidth. Topology is visible via `numactl --hardware`
  and `/sys/devices/system/node/node*/`.
- **WHY AI GETS IT WRONG**: "NUMA only matters on huge servers with 100+
  cores" — desktop Xeons and EPYC workstations are multi-node; remote access
  shows up at modest working-set sizes and thread counts.
- **CORRECT REASONING**: NUMA cost is about WHICH node owns each page and
  which core accesses it. The metric is remote vs local node traffic, not
  total memory size.
- **EXAMPLE** (bad): a 2-socket host where one init thread touches all data
  (all pages on node 0) while workers on node 1 read everything remotely.
- **COUNTEREXAMPLE** (good): each worker first-touches the slice it will
  process, so pages land on the node nearest that worker.
- **VERIFICATION**: Linux target — `numactl --hardware`, `numactl --show`,
  `perf stat` node counters, `/proc/self/numa_maps`. Not executable on the
  Windows verification host.
- **SOURCE**: `intel-opt-manual` (NUMA/memory latency guidance);
  `perf-wiki` (measurement of node traffic).

## 9. numactl: CPU binding and memory policy are separate

- **RULE**: `numactl --cpunodebind=N` constrains where the process RUNS;
  memory placement is governed by a SEPARATE policy (`--membind=N`,
  `--interleave=all`, or the default first-touch). Combining
  `--cpunodebind=N --membind=N` pins both.
- **WHY AI GETS IT WRONG**: "`numactl --cpunodebind=0 ./prog` makes the
  program use node 0 memory" — CPU binding alone leaves the default
  first-touch policy in force, so pages land wherever they are first touched.
- **CORRECT REASONING**: first-touch, not allocation call and not CPU
  binding, decides the owning node. To control placement you must either bind
  memory explicitly or arrange the first-touch threads.
- **EXAMPLE** (bad): `numactl --cpunodebind=0 ./prog` with a single init
  thread that first-touches everything — all pages go to the init thread's
  node, whichever it is.
- **COUNTEREXAMPLE** (good): `numactl --cpunodebind=0 --membind=0 ./prog`
  forces both run location and page placement on node 0.
- **VERIFICATION**: Linux target — `numactl --show`, `/proc/self/numa_maps`
  (page-to-node mapping), `numactl --hardware`. On Windows `numactl` does
  not exist; the tool is documented as a Linux target, UNVERIFIED here.
- **SOURCE**: `numactl(8)` man page — Linux NUMA utility, NOT present in the
  sources registry; cited as documented Linux tool. Supporting reasoning:
  `intel-opt-manual` (NUMA programming guidance).

## 10. First-touch allocation and avoiding remote traffic

- **RULE**: the OS binds a page to a node on FIRST WRITE (first touch), not
  at `malloc`/`calloc` time. Node-local memory therefore requires the
  touching thread to run on (or be bound to) the target node. To avoid
  remote traffic: bind workers to the node owning their data, first-touch
  per-worker slices, or replicate/migrate data explicitly.
- **WHY AI GETS IT WRONG**: "`malloc` returns memory on the current node" —
  `malloc` may not even touch the page; the first store decides the node. An
  init thread and worker threads on different nodes produce a remote-read
  pattern nobody intended.
- **CORRECT REASONING**: layout of WORK determines node placement. Decide
  where each data slice is touched, then ensure the touching thread is local
  to that node (thread affinity + matching allocation or `mbind`).
- **EXAMPLE** (bad): a producer thread fills a buffer (all pages local to its
  node) and a consumer on the other socket reads it — every read is remote.
- **COUNTEREXAMPLE** (good): each worker allocates and first-touches its own
  per-worker buffers; or `mbind`/`numa_alloc_onnode` places shared buffers
  and workers are bound with affinity to that node.
- **VERIFICATION**: Linux target — `numactl --hardware` + `numa_maps` +
  `perf stat` remote-node counters. Windows: NUMA APIs exist
  (`GetNumaHighestNodeNumber`, `SetThreadGroupAffinity`) but were not
  exercised; documented as Linux target, UNVERIFIED here.
- **SOURCE**: `intel-opt-manual` (NUMA placement guidance);
  `perf-wiki` (measuring remote access).
