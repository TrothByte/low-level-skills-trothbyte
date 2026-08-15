# MPI Programming — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to `registry/sources.yaml`.

## 1. MPI_Send blocking behavior is implementation/message-size dependent

- **RULE**: `MPI_Send` may return once the message has been copied into a buffer
  (small messages, eager protocol) or may block until the matching receive has
  posted and the data is transferred (large messages, rendezvous). The standard
  allows either; a correct program must not depend on which.
- **WHY AI GETS IT WRONG**: assumes Send is always non-blocking ("it copies") or
  always blocking; writes code whose correctness relies on eager buffering.
- **CORRECT REASONING**: `MPI_Send` is a *blocking* send in the sense that the
  buffer may be reused only after it returns; the *time* it takes to return is
  undefined. Any algorithm whose liveness depends on Send returning before the
  peer posts Recv is unsafe.
- **EXAMPLE** (bad): two ranks each do `MPI_Send(large)` then `MPI_Recv` — with a
  rendezvous protocol both block forever.
- **COUNTEREXAMPLE** (good): use `MPI_Isend` + `MPI_Irecv` + `MPI_Waitall`, or
  order so the receiving side posts its `MPI_Recv` before the sending side's
  blocking `MPI_Send`.
- **VERIFICATION**: python `examples/good/sim_mpi_deadlock.py` models the
  rendezvous case (Send blocks until Recv posted) and shows the deadlock;
  the reordered version completes.
- **SOURCE**: `mpi-41` §3.4 (blocking send semantics), §3.6 (buffering);

## 2. Two blocking sends with no prior receive deadlock

- **RULE**: if ranks A and B both block on `MPI_Send` to each other before either
  posts its `MPI_Recv`, and the implementation does not buffer the messages, the
  program deadlocks. The classic fix is `Isend`+`Irecv`+`Wait` or posting the
  receive first.
- **WHY AI GETS IT WRONG**: thinks "the runtime handles it"; tests with tiny
  messages that get buffered (eager) and never sees the hang; ships code that
  hangs on a real cluster.
- **CORRECT REASONING**: liveness requires that for every blocking Send there is a
  matching Recv that can be reached without first completing another blocking Send
  with no buffer. Draw the dependency graph; any cycle of blocking Sends with no
  pre-posted receives is a deadlock.
- **EXAMPLE** (bad): `bad/bad_send_send.c` — both ranks `Send` then `Recv`.
- **COUNTEREXAMPLE** (good): both ranks `Irecv` first, then `Isend`, then
  `MPI_Waitall`.
- **VERIFICATION**: sim shows the cycle deadlocks (rendezvous); the Irecv-first
  version completes for all message sizes.
- **SOURCE**: `mpi-41` §3.4-3.5 (blocking send/recv ordering), §3.7 (non-blocking).

## 3. Collectives: same order and same signature on every rank

- **RULE**: collective operations must be called by all ranks of the communicator
  in the same order and with matching arguments (count/datatype/root). A rank that
  skips, duplicates, or reorders a collective breaks the implicit synchronization.
- **WHY AI GETS IT WRONG**: calls `MPI_Allreduce` with rank-dependent counts;
  calls `MPI_Bcast` with different roots; adds a conditional that skips a
  collective on some ranks.
- **CORRECT REASONING**: a collective is an all-rank operation. The "collective
  order" must be identical across ranks, so conditionals around collectives must
  evaluate identically everywhere (or use separate communicators). Any mismatch is
  either a hang or a silent corruption.
- **EXAMPLE** (bad): `if (rank % 2 == 0) MPI_Allreduce(...)` — half the ranks
  never arrive.
- **COUNTEREXAMPLE** (good): all ranks call `MPI_Allreduce` unconditionally with
  the same count.
- **VERIFICATION**: mpirun -np 4 run; hangs on the bad case (documented, no MPI
  here).
- **SOURCE**: `mpi-41` §5 (collectives), §5.2.3 (collective order).

## 4. MPI_Comm_split: color groups, key orders

- **RULE**: `MPI_Comm_split(comm, color, key, &newcomm)` puts ranks with the same
  color into one new communicator; within a color, ranks are ordered by `key`
  (ascending), ties broken by the original rank. Each rank gets rank 0..n-1 in
  its new communicator.
- **WHY AI GETS IT WRONG**: assumes the new communicator keeps the original rank
  numbering, or that color=1 is "one group" — both wrong; assumes key is ignored
  when colors differ.
- **CORRECT REASONING**: color partitions, key orders within a partition. Two
  ranks with different colors are NOT in the same new communicator and cannot
  communicate through it. Compute the intended grouping, then verify the rank
  mapping with `MPI_Comm_rank`.
- **EXAMPLE** (bad): all ranks use `color = 0` and expect a 2-color split.
- **COUNTEREXAMPLE** (good):
  ```c
  int color = rank / 2;          // ranks {0,1}->0, {2,3}->1
  int key = rank % 2;            // order 0,1 within each color
  MPI_Comm_split(MPI_COMM_WORLD, color, key, &newcomm);
  ```
- **VERIFICATION**: mpirun -np 4; print new rank per original rank (documented).
- **SOURCE**: `mpi-41` §6.4 (communicator management, Comm_split).

## 5. Non-blocking: buffer untouched between issue and Wait/Test

- **RULE**: after `MPI_Isend(buf, ...)`/`MPI_Irecv(buf, ...)` the buffer must not
  be read/written until the matching request is completed by `MPI_Wait`,
  `MPI_Test`, or `MPI_Waitall`.
- **WHY AI GETS IT WRONG**: fires `Irecv`, then immediately overwrites the buffer
  before `Wait`; or checks "is it done?" with Test and reuses the buffer anyway.
- **CORRECT REASONING**: the request is a handle; the operation is in flight. The
  buffer is owned by the pending operation. Only `Wait`/`Test` (returned TRUE)
  release it. Reuse-before-complete is a data race on the buffer.
- **EXAMPLE** (bad): `bad/bad_buffer_reuse.c` — `Irecv`, fill the same buffer, wait.
- **COUNTEREXAMPLE** (good): `Irecv` → `Wait` → then fill/consume.
- **VERIFICATION**: TSan (where available) or logic review; documented.
- **SOURCE**: `mpi-41` §3.7 (non-blocking communication).

## 6. MPI-IO: byte offsets and collective participation

- **RULE**: `MPI_File_write_at(fh, offset, ...)`/`read_at` use a byte offset into
  the shared file; collective variants (`MPI_File_write_all`) require every rank
  of the communicator to participate with the same access modes.
- **WHY AI GETS IT WRONG**: writes at `rank * count` without multiplying by the
  element size; calls a collective MPI-IO function from some ranks only; uses
  `MPI_File_seek`+`write` inconsistently across ranks.
- **CORRECT REASONING**: offsets are bytes. `rank*count` is an element offset; the
  byte offset is `rank * count * sizeof(T)`. Collective I/O is a collective: all
  ranks call it, or none. Overlapping/incorrect offsets silently corrupt the file.
- **EXAMPLE** (bad): `MPI_File_write_all(fh, buf, count, MPI_INT, &st);` with
  every rank writing at offset `rank*count` bytes (not elements) — collisions.
- **COUNTEREXAMPLE** (good): `off = (MPI_Offset)rank * count * sizeof(int);`
  `MPI_File_write_at_all(fh, off, buf, count, MPI_INT, &st);`
- **VERIFICATION**: inspect the resulting file on disk (documented).
- **SOURCE**: `mpi-41` §13 (MPI-IO).

## 7. Thread level: MPI_Init_thread, not MPI_Init, in hybrid code

- **RULE**: hybrid MPI+OpenMP code must call `MPI_Init_thread(&argc,&argv,
  MPI_THREAD_FUNNELED, &provided)` (or SERIALIZED/MULTIPLE) and check `provided`.
  Under FUNNELED only the main thread may call MPI; under MULTIPLE any thread may,
  but the library must support it.
- **WHY AI GETS IT WRONG**: calls `MPI_Init` in an OpenMP region; makes MPI calls
  from worker threads with only `MPI_THREAD_SINGLE`.
- **CORRECT REASONING**: the requested level must be at least what the code
  requires. If worker threads call MPI, you need `MPI_THREAD_MULTIPLE` and a
  library compiled for it (check `provided`).
- **EXAMPLE** (bad): `#pragma omp parallel` calling `MPI_Send` after
  `MPI_Init_thread(..., MPI_THREAD_FUNNELED)`.
- **COUNTEREXAMPLE** (good): funneled MPI to the master thread only, or request
  MULTIPLE and verify `provided`.
- **VERIFICATION**: mpirun run; runtime error or deadlock on the bad case.
- **SOURCE**: `mpi-41` §12.4 (thread support).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Send blocking | may buffer (small) or block (large); don't rely on which |
| Deadlock | blocking Send→Send cycle with no pre-posted Recv hangs |
| Collectives | same order + signature on every rank, unconditionally |
| Comm_split | color partitions, key orders within color |
| Non-blocking | buffer untouched between issue and Wait/Test |
| MPI-IO | offsets are bytes; collective I/O = all ranks |
| Hybrid | MPI_Init_thread + correct provided level |
