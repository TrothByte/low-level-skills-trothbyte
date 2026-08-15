# hpc — Skills

Low-level engineering skills for this domain.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `hpc-mpi-programming` | Use when writing, reviewing, or debugging MPI programs: point-to-point Send/Recv and non-blocking Isend/Irecv, collectives, communicators and comm_split, hybrid MPI+OpenMP, MPI-IO, and mpirun/Slurm launch. Prevents deadlocks, ordering bugs, wrong collective counts, and hangs that appear only with 2+ ranks. | researched | `skills/hpc/hpc-mpi-programming` |
| `hpc-openmp-parallel-programming` | Use when writing, reviewing, or debugging OpenMP programs: parallel regions, worksharing loops and schedules, reductions, firstprivate/private/lastprivate, atomic and critical sections, data races on shared variables, and target offload. Prevents races, wrong reduction semantics, and schedule-dependent bugs that only appear with multiple threads. | source-backed | `skills/hpc/hpc-openmp-parallel-programming` |
| `hpc-rdma-verbs` | Use when writing, reviewing, or debugging RDMA programs built on libibverbs: QP/CQ/MR lifecycle, RC/UC/UD transports, work requests and completions, RDMA write/read/atomic, RoCE vs InfiniBand, and perftest interpretation. Prevents QP state-machine violations, rkey misuse, and completion-handling races. | researched | `skills/hpc/hpc-rdma-verbs` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
