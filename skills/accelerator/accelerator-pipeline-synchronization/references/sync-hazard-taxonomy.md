# Synchronization Hazard Taxonomy — Rules

Format per rule: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE.

## 1. Missing sync

- **RULE**: A cross-unit write-read pair on the same buffer with no barrier
  between them is a race. The read may observe stale data.
- **WHY AI GETS IT WRONG**: the program "looks sequential" in the listing, so
  the model assumes sequential execution; it forgets units run concurrently.
- **CORRECT REASONING**: concurrency across units means the listing order is
  not the execution order. Only a barrier creates order.
- **EXAMPLE** (bad): `DMA load -> buf0` followed by `VEC reduce buf0` with
  nothing between (see `examples/bad/bad_missing_sync.py`, hazards on buf0
  and buf1).
- **COUNTEREXAMPLE** (good): a barrier between the DMA write and the vector
  read (see `examples/good/good_sync_schedule.py`).
- **VERIFICATION**: checker reports "no barrier between" for each uncovered
  pair.
- **SOURCE**: arxiv-2605-07881 (missing sync as a named defect class).

## 2. Misplaced sync (barrier on the wrong side of the read)

- **RULE**: A barrier placed after the read it was meant to guard (or before
  the write it should order) does not cover the pair; the hazard remains.
- **WHY AI GETS IT WRONG**: models put "a sync" at stage boundaries or at the
  end of the kernel and then check "sync exists" rather than "sync covers".
- **CORRECT REASONING**: barrier sufficiency is positional: write < barrier <
  read. Verify the interval, not the presence.
- **EXAMPLE** (bad): DMA write, vector read, then a barrier (barrier after
  the read) — see `examples/bad/bad_wrong_sync_order.py`.
- **COUNTEREXAMPLE** (good): the barrier sits strictly between the write and
  the read.
- **VERIFICATION**: checker prints the barrier position and whether it is in
  the (write, read) interval.
- **SOURCE**: arxiv-2605-07881 (misplaced sync as a named defect class).

## 3. Wrong dependency (sync covers the wrong pair)

- **RULE**: A barrier between two unrelated instructions does not order the
  pair that shares a buffer. Coverage is per-pair, not per-stage.
- **WHY AI GETS IT WRONG**: one barrier "between stages" is assumed to order
  everything crossing the stage boundary; it orders only the pairs on either
  side of it.
- **CORRECT REASONING**: enumerate pairs (W, R) on the same buffer; each
  needs its own intervening barrier. A barrier between stage A and stage B
  covers pairs whose write is in A's part and read is in B's part — nothing
  else.
- **EXAMPLE** (bad): a barrier placed between SCL and MAT while the uncovered
  pair is VEC->SCL on buf1.
- **COUNTEREXAMPLE** (good): barriers placed after each producer whose data a
  different unit consumes.
- **VERIFICATION**: checker output lists the offending pair with the barrier
  that "exists but does not cover it".
- **SOURCE**: arxiv-2605-07881 (barrier sufficiency is a per-pair property).

## 4. Reversed same-unit order (write after read in program order)

- **RULE**: Within one unit, program order holds; a read that programmatically
  precedes the write to the same buffer is a programming error, not a race.
- **WHY AI GETS IT WRONG**: generated kernels with fused/merged ops can emit
  a read of a buffer before the write that produces it; the model does not
  check intra-unit dependency direction.
- **CORRECT REASONING**: for same-unit pairs, require write-before-read in
  program order; a barrier cannot fix an intra-unit reversal.
- **EXAMPLE** (bad): within the VEC unit, `read buf3` then `write buf3`.
- **COUNTEREXAMPLE** (good): write to buf3 precedes the read of buf3.
- **VERIFICATION**: checker flags same-unit pairs where the read index is
  below the write index.
- **SOURCE**: arxiv-2605-07881 (program order is one of the three ordering
  relations in the model).

## 5. Sync used as a "memory fence" or timing hack

- **RULE**: Barriers order pipeline stages; they are not the same as memory
  fences, and using them to paper over an ordering bug by inserting delays is
  not verifiable.
- **WHY AI GETS IT WRONG**: models insert sleeps/polls "so the data arrives
  in time"; timing heuristics have no formal basis and break across
  configurations.
- **CORRECT REASONING**: the model's correctness is the structural property
  (barrier sufficiency); anything timing-based is neither sound nor complete.
  On Ascend 910B2 a hazard class produced nondeterministic outputs under one
  toolkit/driver configuration — nondeterminism across configurations is the
  signature of a timing-dependent "fix".
- **EXAMPLE** (bad): `sleep(1)` between stages instead of a barrier.
- **COUNTEREXAMPLE** (good): a proper pipe barrier with verified coverage.
- **VERIFICATION**: run the pipeline across toolchain/driver configurations
  and require identical outputs (determinism check) plus the static check.
- **SOURCE**: arxiv-2605-07881 (nondeterministic outputs observed on Ascend
  910B2 under CANN 8.0.RC3).
