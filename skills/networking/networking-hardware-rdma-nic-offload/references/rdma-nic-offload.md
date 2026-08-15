# RDMA Verbs and NIC Offload — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml. Status tags: KNOWN = verifiable in the verbs headers/
spec/docs; INFERRED = from secondary sources, confirm on the target NIC.

## 1. The verbs object model: PD, QP, MR, CQ, SRQ, AH

- **RULE**: the verbs model is a fixed object graph: device → `ibv_pd`
  (protection domain) scoping resources; `ibv_qp` (queue pair: one send + one
  receive queue, with a state machine); `ibv_mr` (memory region with `lkey`/
  `rkey`); `ibv_cq` (completion queue); `ibv_srq`; `ibv_ah` (address handle
  for UD). Work is *posted* (ibv_post_send/ibv_post_recv) and completion is
  *polled* (ibv_poll_cq). "qpair"/"post_write" are not in the API.
- **WHY AI GETS IT WRONG**: models invent friendly verbs (`ibv_create_qpair`,
  `ibv_post_write`), or collapse MR/PD/CQ into one concept, or describe
  receive via the send path.
- **CORRECT REASONING**: name each object and its lifetime: PD first, MR from
  PD, QP from PD, CQ for completions, then post/poll. An RDMA op needs: an
  SGE (scatter-gather entry: addr/len/lkey), a registered MR backing it, and
  a QP in the right state.
- **EXAMPLE** (bad): `ibv_create_qpair()` / `ibv_post_write(q, ...)`.
- **COUNTEREXAMPLE** (good): `ibv_create_qp(pd, &init_attr)` +
  `ibv_post_send(qp, &wr, &bad)` with `wr.opcode = IBV_WR_RDMA_READ`.
- **VERIFICATION**: compile against `<infiniband/verbs.h>` with `-libverbs`.
  Host stand-in: `python examples/check_verbs_trace.py` validates the QP
  lifecycle on traces. libibverbs absent here — not compiled (researched).
- **SOURCE**: libibverbs (man ibv_create_qp, ibv_post_send, ibv_reg_mr,
  ibv_poll_cq); rdma-verbs-docs.

## 2. QP state machine: RESET → INIT → RTR → RTS

- **RULE**: RC QPs move RESET → INIT (set port, pkey, access flags) → RTR
  (set dest QP number, path MTU, AV, RQ_PSN) → RTS (set SQ_PSN). Any state
  may go to ERROR. `ibv_modify_qp` rejects illegal transitions; a data
  post is only valid once the send queue is armed (RTS).
- **WHY AI GETS IT WRONG**: agents describe "connect the QPs" without the
  four states, or post sends right after create.
- **CORRECT REASONING**: transition legality is the first thing to check in
  any verbs code: `modify_qp(qp, INIT)` requires being RESET; `RTR` requires
  INIT; `RTS` requires RTR. The RTR step is where the *peer's* QP number,
  the path MTU, and the address vector (GID/port) are set.
- **EXAMPLE** (bad): RESET → RTS in one `ibv_modify_qp` call, then
  `ibv_post_send`.
- **COUNTEREXAMPLE** (good): three `ibv_modify_qp` calls (INIT, RTR, RTS),
  then post.
- **VERIFICATION**: the stand-in flags `illegal QP transition RESET->RTS` and
  `post_send ... needs RTS` on `bad/qp_bad_transition.txt` /
  `bad/post_before_rts.txt`. Real check on NIC: failed `ibv_modify_qp`
  returns -EINVAL with `gid/port` errors in dmesg.
- **SOURCE**: libibverbs (man ibv_modify_qp); rdma-verbs-docs.

## 3. MRs, access flags, and one-sided ops (RDMA_WRITE/READ/ATOMIC)

- **RULE**: `ibv_reg_mr` registers memory with access flags:
  IBV_ACCESS_LOCAL_WRITE, REMOTE_WRITE, REMOTE_READ, REMOTE_ATOMIC. The
  local side uses `lkey` in SGEs; the remote side uses the published `rkey`
  + remote address. RDMA_WRITE needs remote_write on the *target* MR,
  RDMA_READ needs remote_read, ATOMIC_FETCH_AND_ADD/ATOMIC_CMP_AND_SWP need
  remote_atomic — otherwise the NIC rejects the op.
- **WHY AI GETS IT WRONG**: models pair an atomic op with a plain MR, or think
  RDMA_WRITE needs no memory registration ("the NIC writes wherever").
- **CORRECT REASONING**: think of access flags as a capability check done by
  the NIC on every remote op: the rkey carries the granted rights. To
  support fetch-and-add you must register the target MR with
  IBV_ACCESS_REMOTE_ATOMIC (and the QP's `qp_access_flags` at INIT).
- **EXAMPLE** (bad): atomic fetch-and-add against an MR registered with only
  local_write.
- **COUNTEREXAMPLE** (good): `ibv_reg_mr(..., IBV_ACCESS_REMOTE_ATOMIC)` then
  `IBV_WR_ATOMIC_FETCH_AND_ADD`.
- **VERIFICATION**: stand-in reports `atomic_fetch_add requires MR
  'remote_atomic'` on `bad/rdma_atomic_no_access.txt`; real check on
  hardware/perftest `ib_atomic_bw`. Researched — no RDMA NIC here.
- **SOURCE**: libibverbs (man ibv_reg_mr); rdma-verbs-docs.

## 4. Transport vs link layer: RC/UC/UD/XRC are not InfiniBand

- **RULE**: "transport" in verbs = the QP service type (RC = reliable
  connected, UC = unreliable connected, UD = unreliable datagram, XRC, SRD);
  "link" = the physical/protocol carrier: native InfiniBand, RoCE (RDMA over
  Converged Ethernet; RoCEv2 uses IP+UDP), or iWARP (over TCP). RC can run
  over any of the three links.
- **WHY AI GETS IT WRONG**: models use "InfiniBand" and "RDMA" and "RC"
  interchangeably and claim RoCE needs no L2/L3 config or that iWARP is the
  same as RoCE.
- **CORRECT REASONING**: separate the two axes in any answer. RoCEv2 requires
  lossless/sufficiently-configured Ethernet (PFC/ECN ideally), a valid GID
  index (v2 uses an IP-based GID), and matching MTU. iWARP terminates in
  software over TCP on many NICs.
- **EXAMPLE** (bad): "RDMA = InfiniBand; RoCE works with zero Ethernet config".
- **COUNTEREXAMPLE** (good): "RC transport over a RoCEv2 link: IP+UDP
  encapsulated, GID index set, Ethernet loss handling configured".
- **VERIFICATION**: `ibv_query_port` (state, link_layer, active_mtu);
  `rdma link show`; `ibv_devinfo -v`. Researched — no RDMA hardware here.
- **SOURCE**: rdma-verbs-docs (RDMA aware programming, RoCE docs);
  nvidia-doca (RoCE configuration guides).

## 5. NIC offload and the "which layer" question

- **RULE**: modern NICs offload: LSO/TSO (segmentation), checksum, GRO/LRO
  (receive coalescing), RSS (flow hashing to queues), flow steering/rte_flow,
  and in switchdev mode an embedded switch (eSwitch) with VF representors.
  DPDK bypasses the kernel stack via vfio-pci and owns the queues; this is
  orthogonal to RDMA (which uses its own queues/verbs).
- **WHY AI GETS IT WRONG**: agents credit offload features to the wrong layer
  (checksum offload "in the socket"), or say DPDK "is" RDMA, or describe
  eSwitch/representors as a kernel bridge feature.
- **CORRECT REASONING**: for each feature name the layer: L2/L3/L4 checksum &
  TSO/GRO = NIC silicon; RSS = NIC + driver indirection table; rte_flow/
  DOCA Flow = control-plane API programming the match/action tables;
  eSwitch = the NIC's internal switching fabric exposed via switchdev
  representors. Verbs are a separate path (queues owned by the HCA).
- **EXAMPLE** (bad): "DPDK handles RDMA traffic" — wrong; verbs handle RDMA.
- **COUNTEREXAMPLE** (good): "for packet flows use rte_flow; for RDMA use
  verbs; the HCA exposes both".
- **VERIFICATION**: `ethtool -k <dev>` (offload feature flags), `ethtool -S`
  (counters), `ibv_devinfo` (verb capabilities). Researched — no NIC with
  these features on this host.
- **SOURCE**: rdma-verbs-docs; nvidia-doca (DOCA Flow, NIC capabilities).

## 6. DOCA and the offload-control API

- **RULE**: DOCA (NVIDIA) layers above verbs and rte_flow: DOCA Flow wraps
  rte_flow for stateful/flow programming, DOCA Comm Channel moves control
  data between host and DPU, DOCA DevInfo enumerates capabilities. It is
  vendor/API specific — "DOCA is generic RDMA" is wrong.
- **WHY AI GETS IT WRONG**: models treat DOCA as a synonym for verbs or DPDK,
  or invent DOCA APIs in answer text.
- **CORRECT REASONING**: cite DOCA only when the question is about NVIDIA
  BlueField/ConnectX offload control; use the documented doca_flow/
  doca_comm_channel symbols and keep verbs for the data path.
- **EXAMPLE** (bad): "implement RDMA_WRITE with DOCA Flow".
- **COUNTEREXAMPLE** (good): "post the RDMA op via verbs; program the NIC's
  match/action via DOCA Flow; no other plumbing".
- **VERIFICATION**: `doca_devinfo`, DOCA SDK samples compiled against
  doca-runtime. Researched — DOCA SDK not installed here.
- **SOURCE**: nvidia-doca (DOCA docs, sample programs).

## Quick reference table

| Claim | Correct fact | Status |
|---|---|---|
| QP lifecycle | RESET→INIT→RTR→RTS→(ERROR) | KNOWN |
| MR access | remote_write/read/atomic as separate flags | KNOWN |
| one-sided ops | lkey local, rkey remote; atomic needs remote_atomic | KNOWN |
| transport vs link | RC/UC/UD = transport; IB/RoCE/iWARP = link | KNOWN |
| RoCEv2 | IP+UDP encapsulation, GID index, lossless Ethernet | KNOWN |
| offload features | TSO/checksum/RSS/eSwitch/rte_flow = NIC layers | KNOWN |
| DPDK vs RDMA | packet queues vs verbs queues; different paths | KNOWN |
| DOCA | vendor API on NVIDIA NICs, not generic | KNOWN |
