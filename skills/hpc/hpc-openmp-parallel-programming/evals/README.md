# Evaluation — hpc-openmp-parallel-programming

Skill: `skills/hpc/hpc-openmp-parallel-programming`.
Stability: `source-backed` for the host-threaded subset. `gcc -fopenmp` (libgomp)
IS available on this machine, so the good/bad threaded examples were actually
compiled and run; outputs recorded below. Target offload (`-fopenmp-targets`,
GPU/nvptx/gcn) is NOT available — the `target`/`map` examples are documentary
(researched — toolchain not available; command: `gcc -fopenmp -fopenmp-targets=
nvptx-none -O2` on a GPU host).

## Toolchain status

- Host OpenMP: `gcc 16.1.0 -fopenmp` + libgomp, verified working 2026-08-15
  (compile + run).
- Target offload / GPU: absent. `good_target_map.c` / `bad_target_map.c` compile
  only with an offload-enabled GCC/Clang; documentary here.
- Data-race detection: `-fsanitize=thread` is not reliably available in MSYS2;
  race detection here is by repeated runs (timing-dependent) + the wrong-result
  oracle, not TSan.

Target commands to promote to `verified` (full matrix):

```
gcc -Wall -Wextra -Werror -O2 -fopenmp examples/good/good_reduction.c -o good_red
OMP_NUM_THREADS=1 ./good_red && OMP_NUM_THREADS=2 ./good_red && OMP_NUM_THREADS=8 ./good_red
# each: sum == 1048576, exit 0
gcc -Wall -Wextra -Werror -O2 -fopenmp examples/bad/bad_race.c -o bad_race
./bad_race          # wrong sum (lost updates), exit 1
gcc -Wall -Wextra -Werror -O2 -fopenmp examples/good/good_atomic_critical.c -o good_at
OMP_NUM_THREADS=8 ./good_at
# offload (GPU host): gcc -fopenmp -fopenmp-targets=nvptx-none examples/good/good_target_map.c
```

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/bad_race.c` | race on shared `sum` → wrong result | COMPILED + RUN: wrong sum (recorded) |
| medium/negative | `bad/bad_private_uninit.c` | `private` read before write (UB) | COMPILED + RUN: happens to give 0 — the "ran fine" trap |
| easy/negative | `bad/bad_atomic_section.c` | compound statement under `atomic` | gcc REJECTS: exit 1 (verified) |
| medium/negative | `bad/bad_target_map.c` | device dereferences unmapped host pointer | documentary; offload absent |
| positive | `good/good_reduction.c` | correct reduction, order-independent | COMPILED + RUN: correct (recorded) |
| positive | `good/good_atomic_critical.c` | reduction(max:) + atomic + critical | COMPILED + RUN: correct (recorded) |
| positive | `good/good_target_map.c` | map(tofrom: a[0:n]) + target reduction | documentary; offload absent |

Detection rule: any shared variable written by more than one thread without
reduction/atomic/critical is a race (UB). Verify with ≥2 threads AND repeated
runs; a single green run is not evidence.

## False-positive evals (correct code must NOT be flagged)

- `good/good_reduction.c` — `reduction(+:sum)` with order-independent body:
  correct.
- `good/good_atomic_critical.c` — `reduction(max:)` for the max, `atomic` on a
  single lvalue (`hits++`), `critical` on a compound update: correct; must NOT be
  "strengthened" to critical everywhere or "simplified" to shared writes.
- A correct `firstprivate` seeding or a `map(to:...)` for a write-only device
  array: must NOT be flagged.
- `OMP_NUM_THREADS`-independent correct results: must NOT be flagged as
  "schedule-sensitive".

## Historical evals

Not applicable as dedicated category: no CVE is attributed to OpenMP constructs
themselves. The failure classes (lost-update races, uninitialized private reads,
invalid atomic forms) are documented from `openmp-spec` §2.17.7/§2.21.1.2 and
reproduced here on the host toolchain.

## Adversarial evals

- `bad/bad_race.c` at NTHREADS=1 would compute the CORRECT sum (single thread,
  no interleaving) — the agent must not certify from a 1-thread run; the
  12-thread run gives 174763 (not 1048576). Repeated runs vary.
- `bad/bad_private_uninit.c` "passes" (prints 0) — uninitialized private reads are
  UB that can look correct; the agent must demand `firstprivate` and
  `-Wmaybe-uninitialized`-clean code.
- A schedule-dependence claim (correctness relying on iteration order) must be
  rejected: correct OpenMP loops are order-independent; only performance varies
  with schedule.

## Verified facts (gcc 16.1.0 -fopenmp, recorded 2026-08-15)

Commands and outputs:

```
$ gcc -Wall -Wextra -Werror -O2 -fopenmp examples/good/good_reduction.c -o good_red   # rc=0
$ ./good_red
good_reduction: threads=12 sum=1048576 (expected 1048576)    # rc=0
$ OMP_NUM_THREADS=1 ./good_red
good_reduction: threads=1 sum=1048576 (expected 1048576)     # rc=0
$ OMP_NUM_THREADS=8 ./good_red
good_reduction: threads=8 sum=1048576 (expected 1048576)     # rc=0

$ gcc -Wall -Wextra -Werror -O2 -fopenmp examples/bad/bad_race.c -o bad_race         # rc=0
$ ./bad_race
bad_race: threads=12 sum=174763 (expected 1048576)           # rc=1  (race -> lost updates)
$ OMP_NUM_THREADS=8 ./bad_race
bad_race: threads=8 sum=131072 (expected 1048576)            # rc=1

$ gcc ... examples/bad/bad_atomic_section.c -o bad_at
error: expected expression before '{' token                   # rc=1 (gcc rejects compound atomic)

$ gcc ... examples/good/good_atomic_critical.c -o good_at     # rc=0
$ ./good_at
good_priv_atomic: max_so_far=999 pair_count=1000 hits=49855  # rc=0
$ OMP_NUM_THREADS=8 ./good_at
good_priv_atomic: max_so_far=999 pair_count=1000 hits=49855  # rc=0
```

Interpretation: the race produces a wrong sum that VARIES with thread count
(174763 vs 131072) while the reduction is correct at 1/8/12 threads — direct
evidence for rules 1-2. gcc rejects the compound `atomic` (rule 4). The
`reduction(max:)`+`atomic`+`critical` program is correct and stable across
thread counts (rule 4-5). Note the bad_private_uninit program compiles and runs
printing 0 — the uninitialized-read trap.

## Scoring (for routing eval)

- recall: race, uninitialized private read, invalid atomic form, and missing map
  detected via reference rules.
- precision: correct reduction/atomic/critical/map patterns produce zero flags.
- FP-rate: zero expected on the good set.

## Target toolchains (absent, documented)

- Offload: `gcc -fopenmp -fopenmp-targets=nvptx-none` or `clang -fopenmp
  --offload-arch=...` on a GPU host.
- TSan: `-fsanitize=thread` where the toolchain supports it (not reliable in
  MSYS2 — documented).
