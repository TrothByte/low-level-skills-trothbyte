# RDMA Verbs (libibverbs) — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to `registry/sources.yaml`.

## 1. QP state machine: RESET → INIT → RTR → RTS, in order, with attributes

- **RULE**: a QP is created in RESET. Legal transitions: RESET→INIT,
  INIT→RTR, RTR→RTS, and any→ERR (or RESET). `ibv_post_send` requires the QP in
  RTS (for SEND/RDMA WRs); `ibv_post_recv` requires ≥ INIT. Each transition
  requires specific `ibv_qp_attr` fields.
- **WHY AI GETS IT WRONG**: posts WRs right after `ibv_create_qp` (RESET); skips
  RTR (goes INIT→RTS directly — illegal); or ignores the attributes needed per
  transition.
- **CORRECT REASONING**: RESET→INIT needs `qp_access_flags` + pkey; INIT→RTR
  needs the remote `dest_qp_num`, the remote address (LID or GID + `ah_attr`),
  `rq_psn`, and depth; RTR→RTS needs `timeout`, `retry_cnt`, `rnr_retry`,
  `sq_psn`, and max WR/sge. Missing fields → `EINVAL` from `ibv_modify_qp`.
  RC/UC need the full remote address; UD needs a path record/AH.
- **EXAMPLE** (bad): `ibv_create_qp` then immediately `ibv_post_send` — QP still
  RESET.
- **COUNTEREXAMPLE** (good):
  ```c
  ibv_modify_qp(qp, &attr, IBV_QP_STATE|IBV_QP_PKEY_INDEX|...);  // INIT
  // ... fill remote addr ...
  ibv_modify_qp(qp, &attr, IBV_QP_STATE|IBV_QP_AV|IBV_QP_DEST_QPN|...); // RTR
  ibv_modify_qp(qp, &attr, IBV_QP_STATE|IBV_QP_TIMEOUT|...);     // RTS
  ```
- **VERIFICATION**: python `examples/good/sim_qp_state.py` checks every transition
  (incl. illegal ones) against the allowed set; recorded output.
- **SOURCE**: `libibverbs` (ibv_create_qp/ibv_modify_qp); `rdma-verbs-docs`
  (QP state machine).

## 2. RDMA WR types need remote addr + rkey

- **RULE**: `IBV_WR_RDMA_WRITE/READ/ATOMIC_*` must set `remote_addr` (a remote
  virtual address) and `rkey` (the remote MR's rkey) on the WR. SEND needs only
  the local sge. The rkey is scoped to a specific registered MR.
- **WHY AI GETS IT WRONG**: forgets the rkey ("it worked with SEND"); uses the
  local lkey for a remote op; or reuses an rkey after the remote MR was
  deregistered.
- **CORRECT REASONING**: RDMA WRs bypass the remote CPU: the remote addr+rkey are
  validated by the HCA against the remote PD's MR. Sharing the rkey out-of-band
  (connection setup) is required; an invalid rkey gives a remote access error
  (completion with error, or QP → ERR).
- **EXAMPLE** (bad): WR.rdma.rkey = 0; WR.rdma.remote_addr = remote_addr.
- **COUNTEREXAMPLE** (good): WR.rdma.rkey = remote_rkey; WR.rdma.remote_addr =
  remote_addr; both obtained during handshake.
- **VERIFICATION**: host model logs "missing rkey" for the bad WR; documentary on
  real HW.
- **SOURCE**: `libibverbs` (ibv_send_wr), `rdma-verbs-docs` (RDMA semantics).

## 3. CQ: poll_cq drains, req_notify_cq arms — both are needed

- **RULE**: `ibv_poll_cq` returns currently-available completions; it does NOT
  block or arm notifications. `ibv_req_notify_cq` arms the event for the next
  completion (one-shot unless `IBV_CQ_SOLICITED` + solicited WRs). The classic
  model: arm (req_notify), poll in a loop, re-arm after draining, and block on the
  CQ event channel between batches.
- **WHY AI GETS IT WRONG**: polls once expecting the completion to be there (it
  may be in flight); arms with no poll loop; or mixes event-channel wait without
  re-arming — the classic missed-completion bug.
- **CORRECT REASONING**: completions arrive asynchronously. Polling only returns
  what has arrived; a busy poll without `req_notify` burns CPU; event-driven wait
  without re-arming misses the NEXT completion. Also check `wc.status ==
  IBV_WC_SUCCESS`; a non-success status is an error completion (with wc.error).
- **EXAMPLE** (bad): `ibv_poll_cq(cq, 1, &wc)` once and treating "no completion"
  as "never completes".
- **COUNTEREXAMPLE** (good):
  ```c
  ibv_req_notify_cq(cq, 0);
  while (ibv_poll_cq(cq, 1, &wc) == 0) ;   // or block on event channel
  if (wc.status != IBV_WC_SUCCESS) handle_error(wc);
  ```
- **VERIFICATION**: host model checks the arm/poll pairing (documentary here).
- **SOURCE**: `libibverbs` (ibv_poll_cq, ibv_req_notify_cq), `rdma-verbs-docs`.

## 4. UD is unreliable; RC is the reliable default

- **RULE**: RC (reliable connected): retransmission, ordering, ACKs — data loss is
  detected and retried by the HCA. UC (unreliable connected): connected but no
  reliability. UD (unreliable datagram): connectionless, no ACK, no ordering, max
  payload ~ MTU (and <= 1 SGE typically, plus the GRH); drops are possible.
- **WHY AI GETS IT WRONG**: uses UD and assumes the peer always receives; or uses
  RC for multicast-like one-to-many (RC is point-to-point only).
- **CORRECT REASONING**: choose by the data's reliability requirement: cannot lose
  data → RC; one-to-many broadcast-like → UD (with your own reliability layer);
  UD messages that exceed the max inline/payload size need segmentation.
- **EXAMPLE** (bad): UD QP, one SEND per "reliable RPC" — messages can be dropped.
- **COUNTEREXAMPLE** (good): RC QP for reliable RPC; UD only for best-effort
  notifications with app-level retransmit.
- **VERIFICATION**: documentary; transport choice rule in the model.
- **SOURCE**: `rdma-verbs-docs` (transport types), `libibverbs`.

## 5. MR lifecycle and rkey scope

- **RULE**: `ibv_reg_mr` pins the buffer and returns lkey+rkey. The rkey is the
  remote-access token: anyone with the rkey can read/write that MR (subject to
  access flags). Deregistering (`ibv_dereg_mr`) invalidates it. Access flags
  (`IBV_ACCESS_REMOTE_WRITE/READ/ATOMIC`) gate remote ops.
- **WHY AI GETS IT WRONG**: registers without REMOTE_WRITE then does remote
  writes; keeps the rkey global; frees the buffer before the remote op completes.
- **CORRECT REASONING**: request the access flags the remote peer will use;
  scope rkey sharing to the peer; keep the MR alive until all WRs referencing it
  have completed (poll CQ).
- **EXAMPLE** (bad): `ibv_reg_mr(pd, buf, len, 0)` then remote RDMA_WRITE to it.
- **COUNTEREXAMPLE** (good): `ibv_reg_mr(pd, buf, len, IBV_ACCESS_REMOTE_WRITE)`.
- **VERIFICATION**: documentary; model flags access-flag mismatch.
- **SOURCE**: `libibverbs` (ibv_reg_mr), `rdma-verbs-docs` (MR semantics).

## 6. RoCE vs InfiniBand: GID vs LID addressing

- **RULE**: IB addresses QPs by 16-bit LID (and optionally GRH); RoCE uses the
  IP/GID addressing (RoCEv2: UDP+IP). RoCE runs over lossy Ethernet and relies on
  PFC/DCQCN/ECN for losslessness. A program that hard-codes LID-based `ah_attr`
  breaks on a RoCE fabric.
- **WHY AI GETS IT WRONG**: treats "RDMA" as one thing; hard-codes `ah_attr.dlid`;
  tests only on IB hardware and ships RoCE-unaware code.
- **CORRECT REASONING**: query the port (`ibv_query_port`): if the link layer is
  `IBV_LINK_LAYER_INFINIBAND` use LID (+ optionally GID); if
  `IBV_LINK_LAYER_ETHERNET` use GID (RoCEv2 with `gid_type` IP-based). Set
  `ah_attr.is_global` accordingly.
- **EXAMPLE** (bad): `attr.ah_attr.dlid = 0x1234;` unconditionally — wrong on RoCE.
- **COUNTEREXAMPLE** (good): branch on `port_attr.link_layer` and set
  `ah_attr.is_global = 1` + `grh` for RoCE.
- **VERIFICATION**: documentary; model checks link-layer selection.
- **SOURCE**: `rdma-verbs-docs` (RoCE vs IB), `libibverbs` (ibv_query_port).

## 7. Resource teardown order

- **RULE**: teardown: deregister MRs, destroy QPs, destroy CQs, free PDs, then
  `ibv_close_device`. Completing WRs must be waited on before their MRs are
  deregistered.
- **WHY AI GETS IT WRONG**: leaks MRs/QPs (kernel resources); frees a buffer while
  a WR still references it.
- **CORRECT REASONING**: the order guarantees no outstanding WR references an
  MR/CQ that is gone. Check every `ibv_*` return code.
- **EXAMPLE** (bad): `free(buf)` right after `ibv_post_send` of an RDMA_WRITE.
- **COUNTEREXAMPLE** (good): poll until the write WR completes, then dereg the MR,
  then free the buffer.
- **VERIFICATION**: documentary.
- **SOURCE**: `libibverbs` (object lifecycle).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| QP states | RESET→INIT→RTR→RTS; post_send only at RTS |
| RDMA WRs | need remote_addr + rkey; SEND needs only sge |
| CQ | poll drains, req_notify arms; check wc.status |
| Transports | RC reliable, UC unreliable-connected, UD datagram |
| MR/rkey | rkey = remote token; keep MR alive until WRs complete |
| RoCE vs IB | IB=LID, RoCE=GID; query link_layer, don't hard-code |
| Teardown | dereg MRs/destroy QP/CQ/PD before close_device |
