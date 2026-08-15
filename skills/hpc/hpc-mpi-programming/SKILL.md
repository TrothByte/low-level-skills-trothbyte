---
name: hpc-mpi-programming
description: Use when writing, reviewing, or debugging MPI programs: point-to-point Send/Recv and non-blocking Isend/Irecv, collectives, communicators and comm_split, hybrid MPI+OpenMP, MPI-IO, and mpirun/Slurm launch. Prevents deadlocks, ordering bugs, wrong collective counts, and hangs that appear only with 2+ ranks.
---

# HPC MPI Programming

## When to use

- Writing or reviewing MPI code that will run under `mpirun`/`mpiexec` or Slurm
  (`srun`) with multiple ranks.
- Debugging a hang that only appears with 2+ ranks (deadlock in p2p or a
  collective-count mismatch).
- Choosing blocking vs non-blocking, and deciding when an `Irecv`/`Isend` pair is
  required to avoid deadlock.
- Using `MPI_Comm_split`, virtual topologies, MPI-IO (`MPI_File_*`), and hybrid
  MPI+OpenMP (threaded MPI vs `MPI_THREAD_FUNNELED`).
- Reviewing an LLM-generated MPI program against the classic MPI failure modes.

## When not to use

- Single-node multithreading — use OpenMP (`hpc-openmp-parallel-programming`).
- GPU collectives — use `gpu-communication-primitives` (NCCL).
- Low-level RDMA verbs — use `hpc-rdma-verbs`; MPI is the portable layer above.
- Pure serial code — MPI is pointless without ranks.

## What the agent often gets wrong

- "`MPI_Send` always blocks until the receiver gets the message." No — with a
  small message it may be buffered and return before the matching recv; with a
  large one it blocks (rendezvous). The program must not depend on either.
- "Blocking `Send`/`Recv` in any order is fine." Two ranks that each block on
  `MPI_Send` before their matching `MPI_Recv` deadlock when the message is large
  (or when the implementation doesn't buffer). This is the classic MPI deadlock.
- "`MPI_Allreduce` can be called on different counts on different ranks." No —
  collectives must be called by all ranks in the same order with the same
  signature, or the program hangs or corrupts.
- "`MPI_Comm_split` creates a new communicator with all ranks." No — it splits by
  color: same color → same new communicator, ordered by key; ranks with different
  colors end up in different communicators.
- "Non-blocking is fire-and-forget." `Isend`/`Irecv` require a matching `Wait`/
  `Test`; using the buffer before the wait is a race.
- "MPI-IO is just fwrite with a rank number." Collective MPI-IO (`MPI_File_write_all`)
  needs all ranks to participate and matching access modes; wrong offset math is a
  classic data-corruption bug.
- "`MPI_Comm_rank` after `MPI_Init` is always 0-based." It is — but with
  `MPI_Comm_spawn`/`Comm_split` new communicators renumber ranks; don't assume
  the original rank ID.

## How to reason correctly

1. Draw the message-passing graph: for each p2p pair, check that every `Send`
   has a matching `Recv` (same tag/comm), and that no rank blocks on a send whose
   peer is also blocked on a send. If both sides use `Send` before `Recv` for a
   large message, reorder or use non-blocking.
2. For collectives, list the call order across ALL ranks; a collective is an
   all-rank operation — a rank that skips or reorders it breaks the barrier
   semantics.
3. For non-blocking, annotate each buffer: allocated, filled, `Isend`, `Wait`,
   reused. The buffer must not be touched between issue and wait.
4. For `Comm_split`, compute color/key per rank explicitly; the result is a new
   communicator whose rank ordering follows key within each color.
5. For MPI-IO, treat `MPI_File_write_at` offsets as byte offsets in the shared
   file; collective variants require uniform participation.
6. For hybrid MPI+OpenMP, choose the thread level (`MPI_Init_thread` with
   `MPI_THREAD_FUNNELED` or `MPI_THREAD_SERIALIZED`); a non-thread-safe MPI call
   from two threads is a bug.

## What to verify

- Every `Send`/`Isend` has a matching `Recv`/`Irecv` with consistent tag, count,
  and datatype.
- No rank waits on a send whose peer waits on a send (blocking deadlock).
- Collectives called by all ranks in the same order, same args.
- Non-blocking buffers untouched between issue and `Wait`/`Test`; `Wait` returned
  before buffer reuse.
- `Comm_split` color/key produce the intended grouping (verify with 2 colors,
  out-of-order keys).
- MPI-IO offsets and access modes correct for all ranks.
- Thread level requested via `MPI_Init_thread`, not plain `MPI_Init`, in hybrid
  code.

## How to verify

```
# Target toolchain (documented; no mpicc/mpirun on this machine):
mpicc -Wall -Wextra -O2 examples/good/good_pingpong.c -o pingpong
mpirun -np 2 ./pingpong            # expect: clean exchange, exit 0
mpirun -np 4 ./good_comm_split     # expect: 2 communicators of 2 ranks each
mpirun -np 2 ./good_nonblocking    # expect: no deadlock with large messages

# Slurm variant on a cluster:
srun -n 4 --ntasks-per-node=2 ./good_collective
```

Toolchain status: no `mpicc`/`mpirun`/`mpiexec` on this machine (win32). All
examples are documentary (researched — toolchain not available; command:
`mpicc && mpirun -np 2`). A Python model of the blocking-Send/Recv rendezvous
deadlock and ordering semantics was run; output in `evals/README.md`.

## Where the knowledge comes from

- `mpi-41` — MPI-4.1 standard: p2p, collectives, non-blocking, comm_split,
  MPI-IO, thread levels.
- `libibverbs` — the verbs layer MPI runs over (for when the agent needs to
  reason about why MPI_Isend is non-blocking).

## Related skills

- `hpc-openmp-parallel-programming` — the thread side of hybrid MPI+OpenMP.
- `gpu-communication-primitives` — same collectives on GPU (NCCL ring).
- `hpc-rdma-verbs` — the transport layer underneath.

## Evaluation

Synthetic: blocking Send/Send deadlock (`bad/bad_send_send.c`), collective count
mismatch (`bad/bad_collective_count.c`), buffer-reuse-after-Irecv
(`bad/bad_buffer_reuse.c`), wrong Comm_split color
(`bad/bad_comm_split_color.c`) — each must be flagged.
False-positive: correct ping-pong, correct non-blocking handshake, correct
comm_split, and correct MPI-IO offsets must NOT be flagged.
Adversarial: a p2p exchange that "works" for small messages (buffered) but
deadlocks for large ones; and an ordering bug that only manifests at 2+ ranks.
Historical: MPI deadlock/ordering classes documented from `mpi-41` §3.
Commands and recorded results: `evals/README.md`.
