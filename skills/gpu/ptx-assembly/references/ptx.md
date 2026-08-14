# PTX Assembly Deep Reference

Source: PTX ISA Specification (`ptx-isa`, v9.3 doc structure) and CUDA C++ Programming
Guide (`cuda-cpp-guide`). Every rule cites the PTX ISA section. Claims marked
[INFERRED] or [SELF-REVIEWED] are not yet confirmed with a running ptxas on this machine.

Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE.

## 1. Module structure: .version, .target, .address_size

- **RULE**: A PTX module starts with `.version X.Y`, then `.target sm_XX`, then optional
  `.address_size 64`. `.address_size` defaults to 32 when omitted. Kernel bodies are
  declared with `.entry name ( .param ... ) { ... }`.
- **WHY AI GETS IT WRONG**: agents copy headers from random snippets; `.address_size`
  is optional and silently defaults to 32, so 64-bit pointer code compiled against a
  32-bit address size is wrong.
- **CORRECT REASONING**: with 64-bit pointers (standard on modern GPUs) you MUST emit
  `.address_size 64`; otherwise pointer registers are treated as 32-bit. The directive
  must immediately follow `.target` if present.
- **EXAMPLE** (bad): `.version 8.0` / `.target sm_80` with no `.address_size`, then
  `ld.param.u64 %rd, [ptr];` — the module assumes 32-bit addresses.
- **COUNTEREXAMPLE** (good):
  ```
  .version 8.0
  .target sm_80
  .address_size 64
  .entry k ( .param .u64 p ) { ... }
  ```
- **VERIFICATION**: `ptxas -arch=sm_80 file.ptx` (target command; not run here).
- **SOURCE**: `ptx-isa` §11.1.1, §11.1.2, §11.1.3, §11.2.1.

## 2. State spaces

- **RULE**: State spaces are `.reg` (registers), `.const` (read-only, cached),
  `.global` (device memory, persistent across launches), `.local` (per-thread private),
  `.param` (kernel/function parameters), `.shared` (per-CTA, on-chip). Texture/surface
  are deprecated. `ld.shared` defaults to `ld.shared::cta`.
- **WHY AI GETS IT WRONG**: "shared memory is visible to the whole grid" — it is NOT;
  it is owned by one CTA (a CUDA thread block) and only cluster peers can see it.
- **CORRECT REASONING**: `.global` is the only state space all threads of all blocks
  share. `.shared` is per-CTA scratch with low latency; it lives/dies with the block.
- **EXAMPLE** (bad): a kernel that expects block 3 to read `sh[]` written by block 0.
- **COUNTEREXAMPLE** (good): use `.global` for inter-block data, `.shared` only for
  within-block cooperation.
- **VERIFICATION**: manual parse; SASS check that shared ops use the shared data path.
- **SOURCE**: `ptx-isa` §5.1, §2.3 (memory hierarchy), §5.1.7.

## 3. Registers and types

- **RULE**: Registers are declared `.reg .type name;` with `.b8/.b16/.b32/.b64`,
  `.u8/.u16/.u32/.u64`, `.s8/.s16/.s32/.s64`, `.f32/.f64`, `.pred`, or vectors.
  Predicate registers are `.reg .pred %p;`. Addresses under `.address_size 64` are `.u64`.
- **WHY AI GETS IT WRONG**: pointer registers declared `.b32`/`.u32` while addressing
  is 64-bit; or predicates used as if they were data registers.
- **CORRECT REASONING**: the register type must hold the operand it carries: 64-bit
  addresses need `.u64`; predicates are a separate `.pred` class written `@p`/`@!p`.
- **EXAMPLE** (bad): `.reg .u32 %ptr; ld.param.u64 %ptr, [p];` — width mismatch.
- **COUNTEREXAMPLE** (good): `.reg .u64 %ptr; ld.param.u64 %ptr, [p];`.
- **VERIFICATION**: `ptxas` type checks operands (target command).
- **SOURCE**: `ptx-isa` §5.1.1, §5.2, §9.3.

## 4. ld/st: state space qualifier and generic addressing

- **RULE**: `ld`/`st` syntax is `ld{.weak}{.ss}{.cop}{.level::...}{.vec}.type` and, for
  the ordering forms, `ld{.sem}.scope{.ss}{...}.type` (same pattern for `st`, with
  `.sem = { .release, .relaxed }`). If NO state space is given, the access uses Generic
  Addressing, which requires a generic address (e.g. produced by `cvta.to.global`).
- **WHY AI GETS IT WRONG**: "missing `.global` just means default" — there is no default
  state space; generic addressing is a different operation with different requirements.
- **CORRECT REASONING**: a shared-space address from `cvta.to.shared` is not a generic
  address; loading it with unqualified `ld.b32` is a programming error. Always qualify
  the space you mean: `ld.shared`, `ld.global`, `ld.local`, `ld.param`, `ld.const`.
- **EXAMPLE** (bad): `ld.b32 %v, [%p_sh];` on an address obtained via
  `cvta.to.shared.u64`.
- **COUNTEREXAMPLE** (good): `ld.shared.u32 %v, [%p_sh];` (see
  `examples/good/good_barrier_shared.ptx`).
- **VERIFICATION**: `ptxas -arch=sm_80` (target), plus manual space audit.
- **SOURCE**: `ptx-isa` §9.7.9.8 (ld), §9.7.9.11 (st), §6.4.1.1 (generic addressing).

## 5. ld/st: data width vs pointer width (.b32 vs .b64)

- **RULE**: the `.type` qualifier of `ld`/`st` is the DATA size, not the pointer size.
  `ld.global.u64` moves 8 bytes; a 32-bit element must be loaded with `.u32`/`.b32`.
  PTX relaxes operand width checks: a destination may be wider than the instruction type,
  but a destination narrower than the instruction type is invalid.
- **WHY AI GETS IT WRONG**: 64-bit pointers make agents write `ld.global.u64` for a
  32-bit `int*`; or they load `.b64` into a `.b32` register expecting it to work.
- **CORRECT REASONING**: pointer width and data width are independent. Read 4-byte
  elements with `.u32`/`.b32`; keep the 64-bit address in a `.u64` register.
- **EXAMPLE** (bad): `ld.global.u64 %v, [%p]; st.global.b64 [%p], %v;` where the element
  is 32-bit — reads 8 bytes and a `b64` store from a 32-bit register is invalid.
- **COUNTEREXAMPLE** (good): `ld.global.u32 %v, [%p]; st.global.u32 [%p], %v;`.
- **VERIFICATION**: `ptxas` rejects narrower destinations (target); `cuobjdump -sass`
  shows `LDG.E` with the width.
- **SOURCE**: `ptx-isa` §9.4.1 (relaxed operand type checking), §9.7.9.8, §9.7.9.11.

## 6. Memory ordering: .weak, .relaxed, .acquire, .release, scope

- **RULE**: plain `ld`/`st` default to `.weak` (no synchronization, no atomicity).
  `.relaxed` adds atomicity but no ordering. Ordering comes from `.acquire` (loads,
  atomics) and `.release` (stores, atomics). `.scope` (`{.cta, .cluster, .gpu, .sys}`)
  names the set of threads that can observe the ordering.
- **WHY AI GETS IT WRONG**: "compiles and usually works" — weak ops let the reader see
  the flag before the data; on real hardware the flag protocol breaks intermittently.
- **CORRECT REASONING**: a release store on M makes prior writes visible to an acquire
  load on M in another thread (release/acquire pattern, PTX ISA §8.8). Both sides must
  participate; `red` does NOT form an acquire pattern (§8.11.1).
- **EXAMPLE** (bad): `st.global.u32 [flag], 1;` then `ld.global.u32 %f, [flag];` in a
  peer — no happens-before edge, data may be stale.
- **COUNTEREXAMPLE** (good):
  ```
  st.global.release.gpu.u32 [%p_flag], 1;   // publisher
  ld.global.acquire.gpu.u32 %f, [%p_flag];  // consumer
  ```
- **VERIFICATION**: `ptxas -arch=sm_80` (target); SASS shows `.ACQUIRE`/`.RELEASE`
  qualifiers.
- **SOURCE**: `ptx-isa` §8.8 (release/acquire patterns), §9.7.9.8, §9.7.9.11, §8.5 (scope).

## 7. Fences: fence.sc, fence.acq_rel, membar

- **RULE**: `fence{.sem}.scope` with `.sem = { .sc, .acq_rel, .acquire, .release }`
  (default `.acq_rel`) and `.scope = { .cta, .cluster, .gpu, .sys }`. `membar.level`
  (`{.cta, .gl, .sys}`) is the older form; on sm_70+ `membar` is a synonym for `fence.sc`
  with levels mapping to `cta/gpu/sys`.
- **WHY AI GETS IT WRONG**: adding a fence "somewhere" and hoping it fixes ordering, or
  using `fence.sc` everywhere (slow, and not always needed).
- **CORRECT REASONING**: `fence.acq_rel` is the light-weight default; `fence.sc`
  restores sequential consistency at a performance cost. Prefer explicit acquire/release
  on the specific operations; use fences when the ordering is not tied to one location.
- **EXAMPLE** (bad): `st.global.u32 [data], %v; fence.acq_rel.cta; st.global.u32
  [flag], 1;` with `.cta` scope while the reader is in another block — the fence does
  not reach the reader.
- **COUNTEREXAMPLE** (good): `fence.acq_rel.gpu;` or, better, a single
  `st.global.release.gpu` on the flag.
- **VERIFICATION**: `ptxas` (target); SASS `MEMBAR`/fence instructions.
- **SOURCE**: `ptx-isa` §9.7.14.4 (membar/fence), §8.9.3 (fence-sc order).

## 8. Atomics: atom, red, cas

- **RULE**: `atom{.sem}{.scope}{.space}.op.type d, [a], b;` — RMW returning the old
  value. Ops: `.and/.or/.xor/.add/.inc/.dec/.min/.max` plus `.cas` (compare-and-swap,
  operands `b`=compare, `c`=swap) and `.exch`. Types `.b32/.b64/.u32/.u64/.s32/.s64/
  .f32/.f64`. Default `.sem = .relaxed`, default `.scope = .gpu`. `red` is the same
  without a destination; its `.sem` is only `{.relaxed, .release}`.
- **WHY AI GETS IT WRONG**: assuming atomics are `seq_cst` by default, or that `.sys`
  is the default scope.
- **CORRECT REASONING**: a bare `atom.add.u32` is `relaxed.gpu`. For a device-wide
  protocol that orders other memory, write `atom.global.acq_rel.gpu.add.u32` (or
  `.release.gpu`) and consume with an acquire. `red` gives no returned value and cannot
  form an acquire pattern.
- **EXAMPLE** (bad): `atom.add.u32 %old, [%p], 1;` as a "flag with data" publish —
  relaxed ordering only.
- **COUNTEREXAMPLE** (good):
  ```
  atom.global.release.gpu.add.u32 %old, [%p_flag], 1;
  atom.global.acq_rel.gpu.cas.b32 %old, [%p_flag], 0, 1;
  red.global.relaxed.gpu.add.u32 [%p], 1;   // fire-and-forget counter
  ```
- **VERIFICATION**: `ptxas` (target); SASS `ATOM`/`RED` with `.RELAXED/.RELEASE`.
- **SOURCE**: `ptx-isa` §9.7.14.5 (atom), §9.7.14.6 (red), §8.11.1.

## 9. Barriers: bar.sync

- **RULE**: `bar{.cta}.sync a{, b};` — 16 barrier resources per CTA (0..15); `b` is the
  thread count (must be a multiple of the warp size; default = all threads in the CTA).
  `bar.sync` makes prior memory accesses performed relative to all participating threads
  and blocks until they arrive. Threads that `exit` release the barrier.
- **WHY AI GETS IT WRONG**: (a) putting `bar.sync` inside divergent code (UB, or
  deadlock); (b) thinking it orders across blocks; (c) mismatched barrier ids or counts.
- **CORRECT REASONING**: every thread that can reach the barrier MUST reach it with the
  same id and count, or behavior is undefined. `bar.sync 0` without a count is the
  `__syncthreads()` pattern and orders shared (and global) accesses within the block.
- **EXAMPLE** (bad): `@%p bar.sync 0;` where `%p` differs between threads of the block.
- **COUNTEREXAMPLE** (good):
  ```
  st.shared.u32 [%p_own], %val;
  bar.sync 0;
  ld.shared.u32 %val, [%p_nbr];
  ```
- **VERIFICATION**: `ptxas` (target); SASS `BAR.SYNC`.
- **SOURCE**: `ptx-isa` §9.7.14.1 (bar/barrier), §3.1.

## 10. Predication and branches

- **RULE**: instructions take an optional guard predicate `@{!}p` (set by `setp`, e.g.
  `setp.lt.s32 p, a, b`). `@p` executes when p is true; `@!p` when false. Conditional
  branches are `@{!}p bra label;`; `bra` alone is unconditional.
- **WHY AI GETS IT WRONG**: inverting the polarity (`@p bra skip` when `@!p bra skip`
  was intended), or treating predication like a jump that keeps the "skipped" code alive.
- **CORRECT REASONING**: a false guard makes the instruction a no-op for that thread;
  it does not branch. Divergent threads then split into reconverging paths — branches
  whose guard differs between threads are the divergence point.
- **EXAMPLE** (bad): `setp.gt.s32 %p, %n, 0; @%p bra $skip; add.s32 %r, %r, %n;` —
  the add runs only when `n <= 0`.
- **COUNTEREXAMPLE** (good): `@!%p bra $skip; add.s32 %r, %r, %n;`.
- **VERIFICATION**: manual; `cuobjdump -sass` shows the predicated/`BRA` instructions.
- **SOURCE**: `ptx-isa` §9.3, §9.7.13.2-3, §9.5 (divergence).

## 11. Integer arithmetic: add, mul, signed vs unsigned

- **RULE**: `add.u32/.s32/.u64/.s64 d, a, b;` and `mul.lo/.hi/.wide` variants.
  Fixed-width integer arithmetic wraps modulo 2^N for both signed and unsigned — PTX has
  no C-style signed-overflow UB. `.s32` vs `.u32` matters only for comparisons
  (`setp.lt.s32` vs `setp.lt.u32`) and conversions.
- **WHY AI GETS IT WRONG**: carrying C's signed-overflow-UB reasoning into PTX, or using
  `.s32` compare where an unsigned compare is required (and vice versa).
- **CORRECT REASONING**: `add.s32 %r, %r, 1;` wraps cleanly. `mul.wide.s32` produces a
  64-bit result from two 32-bit inputs; `mul.hi.u32` gives the high half.
- **EXAMPLE** (bad): `setp.lt.s32 %p, %a, %b;` for a wrap-around index that is unsigned.
- **COUNTEREXAMPLE** (good): `setp.lt.u32 %p, %a, %b;` for unsigned indices.
- **VERIFICATION**: manual; `ptxas` (target).
- **SOURCE**: `ptx-isa` §9.7.1.1 (add), §9.7.1.3 (mul), §9.3.1.1 (comparisons).

## 12. Warp-level ops: shfl.sync, vote.sync, ballot.sync

- **RULE**: `shfl.sync.mode.b32 d[|p], a, b, c, membermask;` (modes `.up/.down/.bfly/
  .idx`); `vote.sync.mode.pred d, {!}a, membermask;` and `vote.sync.ballot.b32`.
  The instruction waits for all threads in `membermask`, which must be the same value in
  every participating lane. UB if the executing thread is not in the mask.
- **WHY AI GETS IT WRONG**: omitting `.sync`, using stale `activemask`, or running the
  shuffle inside divergent code where some mask lanes never arrive.
- **CORRECT REASONING**: all lanes in the membermask must execute the same instruction
  converged, or the behavior is undefined. Use the full-warp mask `0xffffffff` only when
  the whole warp is converged; for masked subsets compute the mask once and reuse it.
- **EXAMPLE** (bad): `shfl.sync.down.b32 %t, %v, 16, 0xffffffff, %activemask;` where a
  masked-off lane later reads the (undefined) source.
- **COUNTEREXAMPLE** (good):
  ```
  shfl.sync.down.b32 %t, %sum, 16, 0xffffffff, 0xffffffff;
  add.u32 %sum, %sum, %t;
  ```
  (full-warp butterfly reduction; lane 0 holds the result.)
- **VERIFICATION**: `ptxas` (target); SASS `SHFL`/`VOTE`.
- **SOURCE**: `ptx-isa` §9.7.9.6 (shfl.sync), §9.7.14.10 (vote.sync).

## 13. How PTX maps to SASS

- **RULE**: PTX is a virtual ISA translated at install time to target SASS by
  ptxas/driver. `ld.global.*` → `LDG.E...`, `ld.shared.*` → `LDS`, `st.shared` → `STS`,
  `atom.*` → `ATOM...`/`RED...`, `bar.sync` → `BAR.SYNC`, `shfl.sync` → `SHFL`,
  `fence/membar` → `MEMBAR`. Cache and ordering qualifiers surface as `.E` suffixes.
- **WHY AI GETS IT WRONG**: quoting specific SASS mnemonics from memory without
  disassembling; the exact form is architecture- and compiler-version-dependent.
- **CORRECT REASONING**: always confirm the final mapping with `cuobjdump -sass`
  instead of predicting it. PTX-level correctness (ordering/scope) is what this skill
  guarantees; SASS specifics are verified per build.
- **EXAMPLE** (bad): claiming `ld.global.acquire.gpu.b32` "always" becomes
  `LDG.E.ACQUIRE.SYS` on every arch. [INFERRED]
- **COUNTEREXAMPLE** (good): run `ptxas -arch=sm_80 x.ptx -o x.cubin && cuobjdump -sass
  x.cubin` and read the actual output. [TARGET]
- **VERIFICATION**: `cuobjdump -sass out.cubin` (target command; not run here).
- **SOURCE**: `ptx-isa` §1.1-1.2 (PTX-to-GPU translator); `cuda-cpp-guide` (compiler
  toolchain).

## 14. Quick detection table

| Bug class | Symptom | Fix |
|---|---|---|
| missing space qualifier | generic addressing on non-generic address | add `.global`/`.shared`/... or `cvta` |
| wrong width | stale/wrong values, invalid dest | match `.type` to data size |
| missing acquire/release | intermittent stale reads | `st.release.gpu` + `ld.acquire.gpu` |
| wrong scope | `.cta` ordering for cross-block peers | `.gpu`/`.sys` |
| missing `bar.sync` | shared-memory race within block | `bar.sync 0` (all threads) |
| divergent `bar.sync` | hang / UB | move barrier out of `@p` code |
| inverted predicate | branch taken backwards | use `@!p` |
| `shfl.sync` mask bug | garbage lanes | uniform membermask, converged lanes |
