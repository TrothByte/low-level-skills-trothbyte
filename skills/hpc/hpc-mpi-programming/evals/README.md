# Evaluation — hpc-mpi-programming

Skill: `skills/hpc/hpc-mpi-programming`.
Stability: `researched` (source-backed grounding: mpi-41, libibverbs). No
`mpicc`/`mpirun`/`mpiexec` on this machine (win32); the `.c` examples are
documentary with target commands recorded. The blocking-send/recv rendezvous
deadlock semantics were verified with a self-contained Python 3.11 model
(`examples/good/sim_mpi_deadlock.py`), actually run; output recorded below. Mark:
SIMULATED — models MPI ordering/buffering semantics, not the MPI runtime.

## Toolchain status

`mpicc`, `mpirun`/`mpiexec`, Slurm: NOT available. Consequences:

- `bad_*.c` and `good_*.c` compile only under an MPI implementation. Target
  commands recorded in each file. NOT run here.
- The Python model reproduces the rendezvous liveness decision (blocking Send
  pending until a matching receive slot is posted) for the two-rank exchange,
  demonstrating both the deadlock and the Irecv-first fix. It does not model the
  runtime, buffering heuristics, or tag/count matching beyond the slot model.

Target commands to promote to `verified` (MPI host):

```
mpicc -Wall -Wextra -O2 examples/good/good_nonblocking.c -o good_nb
mpirun -np 2 ./good_nb              # expect: clean exchange, exit 0
mpicc -Wall -Wextra -O2 examples/good/good_comm_split.c -o good_cs
mpirun -np 4 ./good_cs              # expect: newrank 0,1 per group
mpicc -Wall -Wextra -O2 examples/good/good_mpi_io.c -o good_io
mpirun -np 2 ./good_io              # expect: out.bin with rank-strided data
mpicc examples/bad/bad_send_send.c -o bad_ss
mpirun -np 2 ./bad_ss               # expect: hang (timeout required)
# Slurm: srun -n 4 --ntasks-per-node=2 ./good_io
```

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/bad_send_send.c` | blocking Send/Send deadlock (large msg) | model-checked |
| easy/negative | `bad/bad_collective_count.c` | rank-dependent collective count | review-time flag |
| medium/negative | `bad/bad_buffer_reuse.c` | buffer reused before Wait after Irecv | review-time flag |
| medium/negative | `bad/bad_comm_split_color.c` | all ranks color=0 (one group) | review-time flag |
| positive | `good/good_nonblocking.c` | Irecv-before-Isend, deadlock-free | model-checked |
| positive | `good/good_comm_split.c` | color/key split + collective on newcomm | toolchain absent |
| positive | `good/good_mpi_io.c` | byte offsets = rank*count*sizeof(int) | toolchain absent |

## False-positive evals (correct code must NOT be flagged)

- `good/good_nonblocking.c` — Irecv/Isend/Waitall with buffers untouched until
  completion: correct; must NOT be flagged as "missing synchronization".
- `good/good_comm_split.c` — a correct color/key split followed by a collective
  on the new communicator: correct.
- `good/good_mpi_io.c` — byte offsets and collective `write_at_all`: correct.
- Blocking `MPI_Send` with a pre-posted `MPI_Recv` (no deadlock) must NOT be
  "fixed" to non-blocking.

## Historical evals

Not applicable as dedicated category: no CVE is attributed. The deadlock and
ordering failure classes are documented from `mpi-41` §3 (p2p) and §5
(collectives). A historical CVE corpus for MPI bugs (e.g. MPI_Allreduce count
mismatch hangs) is out of scope until an MPI host exists.

## Adversarial evals

- A p2p exchange that works for small (buffered) messages but deadlocks for large
  (rendezvous) ones — the model shows why: Send stays pending without a posted
  slot. The agent must demand large-message testing and the Irecv-first pattern.
- A collective that is correct at rank counts that divide evenly but mismatched at
  others — require identical signatures and unconditional participation.
- An ordering bug that only manifests at 2+ ranks (any rank count > 1) — the
  agent must test with `-np 2` minimum, never `-np 1`.

## Verified facts (python 3.11.9 run, recorded 2026-08-15)

Command: `python examples/good/sim_mpi_deadlock.py`

```
MPI blocking-send semantics model (mpi-41 §3.4)

blocking Send/Send, no pre-posted Recv: DEADLOCK
Irecv first, then Isend+Wait:            COMPLETE

Model: PASS (deadlock reproduced, reorder fixes it)
Model of MPI ordering semantics — not the runtime. Documented target: mpicc && mpirun -np 2.
```

Interpretation: with rendezvous semantics a blocking Send whose peer has no
receive slot stays pending forever (both ranks block → DEADLOCK). Posting the
receive first makes the peer's send deliver immediately → COMPLETE. This is the
rule-1/rule-2 liveness reasoning of `references/mpi-rules.md`.

## Scoring (for routing eval)

- recall: deadlock, collective-count mismatch, buffer reuse, and comm_split bugs
  detected via the reference rules.
- precision: correct non-blocking, comm_split, and MPI-IO patterns produce zero
  flags.
- FP-rate: zero expected on the good set.

## Target toolchains (absent, documented)

- `mpicc` + `mpirun -np 2` (or `mpiexec`): OpenMPI/MPICH host needed.
- `srun` (Slurm): cluster launcher for the documented variant.
- Python 3.11 deadlock model: AVAILABLE, run, recorded above.
