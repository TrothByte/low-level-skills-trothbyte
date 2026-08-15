---
name: hpc-rdma-verbs
description: Use when writing, reviewing, or debugging RDMA programs built on libibverbs: QP/CQ/MR lifecycle, RC/UC/UD transports, work requests and completions, RDMA write/read/atomic, RoCE vs InfiniBand, and perftest interpretation. Prevents QP state-machine violations, rkey misuse, and completion-handling races.
---

# HPC RDMA Verbs (libibverbs)

## When to use

- Writing code using `ibv_open_device`, `ibv_alloc_pd`, `ibv_reg_mr`,
  `ibv_create_cq`, `ibv_create_qp`, `ibv_post_send`/`ibv_post_recv`,
  `ibv_poll_cq`/`ibv_req_notify_cq`.
- Choosing and configuring QP type: RC (reliable connected), UC (unreliable
  connected), UD (unreliable datagram).
- Reasoning about RDMA WR types: `IBV_WR_SEND`, `IBV_WR_RDMA_WRITE`,
  `IBV_WR_RDMA_READ`, `IBV_WR_ATOMIC_CMP_AND_SWP`, and the rkey/remote address.
- Deciding RoCE vs InfiniBand (LID vs GID addressing, PFC vs lossless IB).
- Interpreting `ib_write_bw`/`ib_read_lat`/`perftest` output, or diagnosing a
  completion error (`ibv_wc` status).

## When not to use

- Portable MPI code — use `hpc-mpi-programming`; MPI hides verbs.
- GPU collectives — use `gpu-communication-primitives` (NCCL).
- Regular sockets — this is RDMA-specific.
- Kernel drivers (RDMA/cma, rdma-core kernel side) — out of scope here.

## What the agent often gets wrong

- "Post sends before the QP is in the right state." `ibv_post_send` requires the
  QP in `IBV_QPS_RTS` (and posts a `IBV_WR_SEND` only after RTR->RTS); posting on
  an INIT/ERR QP returns an error or undefined behavior.
- "QP state changes happen instantly." `ibv_modify_qp` is a state machine:
  RESET → INIT → RTR → RTS (and ERR/RESET on error). Attributes must be
  correct per transition (e.g. RTR needs the peer's QPN + address).
- "MR registration is all I need for a remote access." RDMA READ/WRITE/ATOMIC
  needs the remote rkey AND the remote address (addr + `lkey`/`rkey`). The rkey
  is the authorization token — sharing it grants access to that MR.
- "UD is reliable enough." UD is unreliable and connectionless: no retransmission,
  no ACKs, max message size limited by MTU; you must handle drops.
- "A completion is generated for every send immediately." A send completes when
  the WORK REQUEST is done (data placed), which for SEND means after the peer
  receives it; for RDMA_WRITE it's when the write is flushed. Poll until
  `ibv_wc.status == IBV_WC_SUCCESS`.
- "`ibv_poll_cq` is the same as `ibv_req_notify_cq`." `poll_cq` drains; `req_notify`
  arms the notification for the NEXT completion (one-shot unless `IBV_CQ_SOLICITED`
  and solicited completions). Both are needed for the classic polling model.
- "RoCE and IB are interchangeable." IB uses LIDs (16-bit) for addressing; RoCE
  uses GIDs/IPv6 over lossy Ethernet (needs PFC or DCQCN for reliability). A
  program that hard-codes a LID breaks on RoCE.
- "`ibv_alloc_pd`/`ibv_reg_mr` are free." They allocate kernel resources; MRs must
  be deregistered and QPs destroyed before `ibv_close_device`.

## How to reason correctly

1. Draw the QP state machine and label which attributes each transition needs:
   RESET→INIT (pkey, access flags), INIT→RTR (peer QPN, address, rq_depth,
   rnr retry), RTR→RTS (timeout, retry count, rnr, sge limits). Posting WRs
   before RTS is a bug.
2. For each WR, list (op, local sge, remote addr+rkey). SEND needs no remote
   addr; RDMA_WRITE/READ/ATOMIC need `remote_addr` + `rkey` on the remote QP's PD.
   The rkey must be shared explicitly (out-of-band) and scoped to the MR.
3. Track completions: CQ + WR completion count. A WR completes when `poll_cq`
   returns a WC for it with SUCCESS; with `IBV_CQ_SOLICITED` notify only on
   solicited events. Never free an MR/address until all WRs using it completed.
4. Pick the transport by reliability + message size: RC (reliable, connected,
   up to 1GB via multiple WRs), UC (unreliable connected, no ACK), UD (datagram,
   up to MTU-ish payload). For data you cannot afford to lose, RC.
5. For RoCE vs IB, enumerate the addressing: IB LID-based, RoCE GID-based. Check
   the transport type in the device attributes; don't hard-code the address type.
6. For perftest, read the header: message size, QP count, and the final
   bandwidth/latency lines — and verify the run matched (e.g. -x shared memory
   off, -d device, -i GID index on RoCE).

## What to verify

- QP state transitions issued in order with the required attributes; `ibv_modify_qp`
  return checked; no `post_send` before RTS.
- Every WR has valid sge; RDMA WRs carry a real remote addr+rkey.
- rkey scope: the MR covers the remote address; the peer knows the rkey (it was
  shared out-of-band).
- Completions: WRs waited for; `ibv_wc.status` checked; MRs deregistered only
  after completion.
- Resource teardown: destroy QP, CQ, dereg MR, free PD, close device — no leaks.
- Transport choice matches the reliability requirement; UD message size respected.

## How to verify

```
# Target toolchain (documented; no RDMA hardware/verbs on this machine):
gcc -Wall -Wextra -O2 -libverbs examples/good/good_qp_setup.c -o good_qp
# run two nodes (or loopback if mlx4/5 + roce/ib configured):
./good_qp server <gid-or-lid> &
./good_qp client <gid-or-lid>
# perftest on the cluster:
ib_write_bw -d mlx5_0 -i 1  <server-ip>
ib_read_lat -d mlx5_0 -i 1  <server-ip>
```

Toolchain status: no RDMA hardware, no libibverbs on this machine (win32). All
examples are documentary (researched — toolchain not available; command:
`gcc -libverbs` on a host with mlx4/mlx5 or similar). A Python model of the QP
state machine and WR/completion lifecycle was run; output in `evals/README.md`.

## Where the knowledge comes from

- `libibverbs` — the verbs API (QP/CQ/MR, ibv_post_send/recv, ibv_poll_cq).
- `rdma-verbs-docs` — RDMA/InfiniBand semantics: RC/UC/UD, RoCE vs IB, rkey.
- `mpi-41` — how MPI maps onto verbs, for the "why" of reliability.

## Related skills

- `hpc-mpi-programming` — portable message passing over verbs.
- `gpu-communication-primitives` — NCCL sits on top of verbs on the GPU side.
- `concurrency-deadlock-and-lock-ordering` — completion-polling loops share the
  discipline.

## Evaluation

Synthetic: post_send before RTS (`bad/bad_post_send_state.c`), RDMA write without
rkey (`bad/bad_missing_rkey.c`), poll with no notify / notify with no poll
(`bad/bad_cq_poll.c`), UD treated as reliable (`bad/bad_ud_reliable.c`) — each
must be flagged.
False-positive: correct RESET→INIT→RTR→RTS sequence, correct rkey usage, correct
poll+notify loop, and correct RC selection must NOT be flagged.
Adversarial: a program that "works on loopback" (GID vs LID hard-coded) but breaks
when the transport is RoCE; and a completion race where the MR is deregistered
before the last WR completes.
Commands and recorded results: `evals/README.md`.
