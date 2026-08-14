# Evaluation — ptx-assembly

Skill: `skills/gpu/ptx-assembly`. Stability: `draft` → `researched` (not yet
`source-backed`/`evaluated`: no GPU toolchain on this machine; see Toolchain status).

## Toolchain status

`nvcc`, `ptxas`, `cuobjdump` were NOT found on this machine (`nvcc --version`,
`ptxas --version` returned "not recognized"). Consequences, stated honestly:

- The `.ptx` example files were written against the PTX ISA 9.3 grammar and
  cross-checked against the spec text (sections listed in `references/ptx.md`).
  That validation is **self-reviewed**, not tool-verified.
- Claims marked `[INFERRED]` or `[SELF-REVIEWED]` in the reference are not confirmed by
  a running assembler. Everything else was checked against the PTX ISA primary source
  text (loaded and read during authoring): ld/st syntax and defaults (§9.7.9.8/11),
  atom/red default `.sem=.relaxed` / `.scope=.gpu` (§9.7.14.5/6), fence and membar
  (§9.7.14.4), bar.sync guarantees (§9.7.14.1), shfl.sync/vote.sync membermask rules
  (§9.7.9.6/§9.7.14.10), predication (§9.3), relaxed operand-width rules (§9.4.1),
  release/acquire patterns and the "red forms no acquire pattern" rule (§8.8, §8.11.1),
  `.shared::cta` default (§5.1.7), `.address_size` default 32 (§11.1.3).
- Target commands to promote to `verified` (run on a machine with the CUDA toolkit):

```
ptxas -arch=sm_80 examples/good/good_acquire_release_flag.ptx -o /tmp/a.cubin
ptxas -arch=sm_80 examples/good/good_atomic_add.ptx         -o /tmp/b.cubin
ptxas -arch=sm_80 examples/good/good_barrier_shared.ptx     -o /tmp/c.cubin
ptxas -arch=sm_80 examples/good/good_shfl_warp_reduction.ptx -o /tmp/d.cubin
cuobjdump -sass /tmp/a.cubin
```

Expected results (must confirm): good files assemble; `cuobjdump -sass` shows
`LDG.E.ACQUIRE`/`STG.E.RELEASE` on the flag ops, `ATOM`/`RED` on the atomics,
`BAR.SYNC` on the barrier, `SHFL` on the shuffles. The bad files should fail or be
flagged by code review as described below.

## What was verified (this session)

- PTX ISA primary source consulted directly: every normative claim in
  `references/ptx.md` cites a real section of the PTX ISA spec (see Toolchain status).
- Manual parse of all 9 example files against that grammar (self-reviewed).
- Verified facts that do NOT need a GPU toolchain:
  - plain `ld`/`st` default to `.weak` (no synchronization);
  - `atom`/`red` default to `.relaxed` ordering, `.gpu` scope;
  - `fence` defaults to `.acq_rel`; `membar` is a synonym for `fence.sc` on sm_70+;
  - `.shared` defaults to `::cta`; `.address_size` defaults to 32;
  - `bar.sync` requires every reachable thread with matching id/count (count is a
    multiple of warp size); 16 barriers (0..15) per CTA;
  - `shfl.sync`/`vote.sync` are UB when the executing thread is not in the membermask;
  - `ld`/`st` destinations may be wider than the instruction type, but narrower is
    invalid (`.b32` vs `.b64` confusion);
  - `red` cannot form an acquire pattern.

## Synthetic evals

- **easy/negative**: a kernel with `ld.global.u64` on a `u32` element (data width vs
  pointer width) — must flag and fix to `ld.global.u32`.
- **easy/negative**: `ld.b32` on a shared address with no `.shared` qualifier — must
  flag the missing space qualifier (generic vs shared addressing).
- **medium/negative**: cross-CTA flag protocol using plain `st.global`/`ld.global`
  (`examples/bad/bad_flag_ordering.ptx`) — must detect the missing release/acquire edge
  and fix with `st.global.release.gpu` + `ld.global.acquire.gpu`.
- **medium/negative**: shared-memory exchange without `bar.sync`
  (`examples/bad/bad_missing_barrier.ptx`) — must flag the missing barrier and insert
  `bar.sync 0` after the store.
- **hard/negative**: inverted predicate polarity (`examples/bad/
  bad_predicate_semantics.ptx`) — must reason that `@%pgt bra $skip` executes the body
  only when `n <= 0`, fix to `@!%pgt`.
- **adversarial**: a `shfl.sync` call with a stale/wrong membermask inside divergent
  code — compiles, "works" on some lanes, returns garbage on others; must identify the
  convergence requirement.
- **ambiguous**: `fence.acq_rel.gpu` placed between two weak accesses vs explicit
  acquire/release on the accesses — the agent must explain both are correct patterns and
  mark the choice OPTIONAL (not a bug).

## False-positive evals (correct code must NOT be flagged)

- Correct `ld.global.acquire.gpu.b32` / `st.global.release.gpu.b32` pair
  (`examples/good/good_acquire_release_flag.ptx`) — must NOT be "strengthened" to
  `fence.sc` or `.sys` scope.
- `atom.global.add.u32` for a plain statistics counter — relaxed is legitimate; must NOT
  be flagged for missing acquire/release.
- `bar.sync 0`-guarded shared exchange (`examples/good/good_barrier_shared.ptx`) — must
  NOT be flagged for a "missing" atomic; a barrier is the correct tool within a block.
- `shfl.sync` with full-warp mask `0xffffffff` in converged code — must NOT be flagged.
- A kernel that only uses `.global` and `.reg` (no shared, no atomics) — must NOT be
  flagged for "missing synchronization".
- Metrics: precision, recall, false-positive rate, warning density (per
  `registry/evals.yaml` false-positive rule).

## Scoring

- detection: names the missing qualifier/ordering/barrier/mask and the concrete
  instruction affected.
- reasoning: explains the mechanism (generic vs shared addressing; release/acquire edge;
  barrier participation; membermask convergence), not just "add a barrier".
- fix: minimal change at the right instruction; does not over-strengthen (e.g. no
  blanket `fence.sc.sys`).
- verification: cites `ptxas -arch=sm_80 <file>.ptx` and `cuobjdump -sass` as the
  target verification and states that the current machine lacks the toolchain.

## Promotion checklist (to `source-backed`/`evaluated`)

1. Run the four `ptxas` commands above on a CUDA machine; all good files assemble.
2. Confirm the bad files fail (size-type, missing-space) or assemble but are flagged by
   the reference rules (ordering, barrier, predicate) in review.
3. `cuobjdump -sass` confirms `LDG.E.ACQUIRE`/`STG.E.RELEASE`/`ATOM`/`BAR.SYNC`/`SHFL`.
4. Re-verify the `[INFERRED]` SASS-mapping note (§13) against real output.
