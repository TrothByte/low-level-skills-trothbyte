# AccelSync Model — Rules

Format per rule: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE.

## 1. Missing or misplaced synchronization escapes simulation and golden testing

- **RULE**: Accelerator operators compile into multi-stage pipeline programs
  where DMA, vector, matrix, and scalar units execute concurrently on shared
  on-chip buffers. A missing or misplaced synchronization primitive creates
  hardware-visible data races that escape both simulation and golden testing,
  because neither models the accelerator's cross-unit visibility semantics.
- **WHY AI GETS IT WRONG**: "passes simulation" and "passes golden" are
  treated as correctness evidence. A golden run is one sampled interleaving;
  if that interleaving happens to be ordered, the race is invisible.
- **CORRECT REASONING**: correctness is a property of the program under all
  interleavings the hardware allows, not of the one schedule a test executed.
  The race is real if ANY allowed interleaving reads stale data — even if
  every test run so far was lucky.
- **EXAMPLE** (bad): DMA load writes `buf0`; the vector unit reads `buf0`
  with no barrier between; the golden test's single run scheduled the DMA
  first and passed.
- **COUNTEREXAMPLE** (good): a barrier strictly between the DMA write and the
  vector read forces the ordered interleaving; all interleavings agree.
- **VERIFICATION**: enumerate (or statically check) the interleavings a
  barrier-allowed schedule permits and show the stale-read one exists
  (`examples/bad/bad_missing_sync.py`).
- **SOURCE**: arxiv-2605-07881 (AccelSync), abstract: "A missing or misplaced
  synchronization primitive introduces hardware-visible data races that
  escape both simulation and golden testing, because neither models the
  accelerator's cross-unit visibility semantics."

## 2. Barrier sufficiency is the correctness property

- **RULE**: Formalize the pipeline as a restricted concurrent language with
  program order, synchronization order, and barrier order. Correctness
  reduces to barrier sufficiency: every cross-unit write-read pair on the same
  buffer must be ordered by happens-before. Barrier sufficiency is decidable
  in O(|E|^2) and the check is sound and complete under the modeled semantics.
- **WHY AI GETS IT WRONG**: reviewing "there is a barrier" instead of "every
  write-read pair is covered by a barrier" — existence of sync is not coverage
  of sync.
- **CORRECT REASONING**: enumerate cross-unit write-read pairs on each shared
  buffer; for each pair require a barrier strictly between write and read.
  Same-unit pairs are ordered by program order instead. This coverage check
  is the whole correctness question.
- **EXAMPLE** (bad): a program with one barrier per stage, while two pairs
  (DMA->VEC on buf0, VEC->SCL on buf1) have no barrier between them.
- **COUNTEREXAMPLE** (good): a barrier after each cross-unit write so every
  pair has an intervening barrier; the checker reports 0 hazards.
- **VERIFICATION**: run the checker on
  `examples/good/good_sync_schedule.py` (SAFE) and
  `examples/bad/bad_missing_sync.py` (UNSAFE).
- **SOURCE**: arxiv-2605-07881, abstract: "reduce the correctness question to
  barrier sufficiency... decidable in O(|E|^2) time... both sound and complete
  under the modeled semantics."

## 3. Placement matters: a barrier after the read guards nothing

- **RULE**: A barrier orders events before it against events after it. To
  order a write W before a read R, the barrier must sit strictly between W and
  R in program sequence. A barrier after R (or before W) covers the pair
  vacuously.
- **WHY AI GETS IT WRONG**: LLM-generated kernels place sync "at the end of
  the stage" or "at the end of the kernel", which is after the reads that
  needed it — the hazard remains but "a barrier exists".
- **CORRECT REASONING**: check the relative positions: write < barrier <
  read. If the barrier is outside that interval for the pair, the pair is
  unordered regardless of the barrier's presence.
- **EXAMPLE** (bad): DMA write at 0, vector read at 1, barrier at 2 — the
  barrier is after the read it should have guarded.
- **COUNTEREXAMPLE** (good): DMA write at 0, barrier at 1, vector read at 2.
- **VERIFICATION**: `examples/bad/bad_wrong_sync_order.py` reports the two
  hazards with the barrier's useless position annotated.
- **SOURCE**: arxiv-2605-07881 (barrier order semantics; "misplaced
  synchronization primitive").

## 4. Scope: all cross-unit pairs, including DMA loads and stores

- **RULE**: DMA is a unit like any other. A DMA store reading a buffer that a
  matrix unit is still writing is a race; a DMA load feeding a vector unit
  without a barrier is a race. Sync coverage must include the memory
  interface.
- **WHY AI GETS IT WRONG**: LLM-generated kernels often sync only between
  compute units and treat the DMA load/store as instantaneous or "already
  done".
- **CORRECT REASONING**: model the DMA load as a unit write to a buffer and
  the DMA store as a unit read from a buffer; run the same coverage check.
- **EXAMPLE** (bad): DMA store of `buf4` with the matrix unit's write to
  `buf4` unbarrier'd.
- **COUNTEREXAMPLE** (good): barrier between the matrix write and the DMA
  store read.
- **VERIFICATION**: checker includes DMA in the unit set
  (`examples/good/good_sync_schedule.py` ends with a barrier before the DMA
  store).
- **SOURCE**: arxiv-2605-07881 ("DMA, vector, matrix, and scalar units
  execute concurrently on shared on-chip buffers").

## 5. Incidence: LLM-generated kernels are the high-risk population

- **RULE**: On 120 LLM-generated kernels, AccelSync flagged a 19.2% defect
  rate (95% CI [13.0%, 27.4%]); a mutation study of 688 non-equivalent
  mutants achieved 100% detection; it also found 3 previously unknown hazards
  in 6,292 production CANN kernels and detects hazards msSanitizer misses at
  400x lower cost per kernel.
- **WHY AI GETS IT WRONG**: agents believe their generated pipelines are
  correct because they "compiled" and "ran once"; the defect rate says
  otherwise.
- **CORRECT REASONING**: treat generated pipeline code as high-risk: run a
  structural coverage check on every program, independent of vendor
  sanitizers and of golden runs.
- **EXAMPLE** (bad): shipping an LLM-generated kernel after one successful
  golden run, no coverage check.
- **COUNTEREXAMPLE** (good): static barrier-sufficiency check in the CI gate
  before deployment.
- **VERIFICATION**: apply the check in this skill's examples to any generated
  kernel under review.
- **SOURCE**: arxiv-2605-07881, abstract.
