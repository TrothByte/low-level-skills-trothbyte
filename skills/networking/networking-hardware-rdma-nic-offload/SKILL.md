---
name: networking-hardware-rdma-nic-offload
description: Use when writing or reviewing RDMA verbs code or NIC offload claims — QP/MR/CQ semantics, transport vs link layer (RC/RoCE/iWARP/InfiniBand), one-sided operations, and DPDK/eSwitch/rte_flow offload. Prevents invented verbs APIs and offload features misattributed to the wrong layer.
---

# RDMA Verbs and NIC Offload

## When to use

- Writing or reviewing verbs code: PD/QP/MR/CQ/SRQ, post_send/post_recv/poll_cq,
  QP state transitions, RDMA_READ/WRITE/ATOMIC.
- Explaining RDMA transport vs link layers (RC/UC/UD/XRC over InfiniBand,
  RoCE, or iWARP) and RoCEv2 configuration.
- Describing NIC offload: TSO/GRO/RSS, flow steering/rte_flow, eSwitch/
  representors, DPDK, SR-IOV, or DOCA on NVIDIA NICs.
- Debugging "verbs op fails on the NIC": illegal QP transitions, missing MR
  access flags, wrong GID index.

## When not to use

- Regular TCP/UDP networking or sockets — no verbs involved.
- Kernel container/cgroup behavior — use `kernel-container-internals`.
- Writing a full RDMA application from scratch without hardware — this skill
  validates claims; a real testbed is still required.

## What the agent often gets wrong

- Inventing verbs API names (`ibv_create_qpair`, `ibv_post_write`) that do
  not exist in `<infiniband/verbs.h>`.
- Skipping QP states: posting sends before RTS, or one `ibv_modify_qp` jump
  RESET→RTS.
- Pairing atomic ops with MRs lacking IBV_ACCESS_REMOTE_ATOMIC, or RDMA_WRITE
  with no registered MR.
- Treating "InfiniBand", "RDMA", "RC", and "RoCE" as synonyms and claiming
  RoCE needs zero Ethernet configuration.
- Crediting offload features to the wrong layer ("checksum offload in the
  socket") or saying "DPDK is RDMA".
- Treating DOCA as a generic RDMA term instead of a vendor API.

## How to reason correctly

1. Anchor every claim in the verbs object model: PD scopes MR/QP; QP has a
   state machine; completions arrive on CQ; nothing happens until posted.
2. For any QP action, check the state machine: RESET→INIT→RTR→RTS; data
   posts need RTS, receives need RTR.
3. For one-sided ops, name the MR and its flags: lkey local, rkey remote;
   remote_read/remote_write/remote_atomic gate what the peer may do.
4. Separate transport (RC/UC/UD/XRC — the QP type) from link (IB/RoCEv2/
   iWARP — the carrier), and say which GID/MTU/loss config applies.
5. For offload, name the layer: silicon (checksum/TSO/GRO/RSS), control API
   (rte_flow/DOCA Flow), data path (verbs or DPDK queues).
6. If a claim needs a NIC, say what to query (`ibv_devinfo`,
   `ethtool -k`), not what the NIC "should" do.

## What to verify

- QP transitions are legal and in order.
- Each remote op has the matching MR access flag.
- Transport and link layers are named separately and consistently.
- Offload features are attributed to the correct layer.
- Any verbs/offload API name exists in the actual headers/docs.

## How to verify

```
ibv_devinfo -v                # HCA caps, link layer, active MTU
rdma link show
ethtool -k <dev>              # offload feature flags
ethtool -S <dev>              # NIC counters
perftest: ib_write_bw -a -d mlx5_0   # data-path sanity on hardware
gcc -libverbs -o app app.c     # compile against <infiniband/verbs.h>
python examples/check_verbs_trace.py good/qp_rc_trace.txt
python examples/check_verbs_trace.py bad/*.txt   # host stand-in
```

On this host there is no RDMA NIC, no libibverbs, and no perftest — the
compiled-on-Linux and hardware commands above are researched and documented,
not run. `check_verbs_trace.py` is a host-side stand-in for the QP-state and
MR-access rules and was actually executed.

## Where the knowledge comes from

- `libibverbs` — man pages: ibv_create_qp, ibv_modify_qp, ibv_reg_mr,
  ibv_post_send/recv, ibv_poll_cq, ibv_alloc_pd.
- `rdma-verbs-docs` — RDMA-aware programming, QP states, one-sided ops,
  RoCE/iWARP/InfiniBand link layers.
- `nvidia-doca` — DOCA Flow, DOCA Comm Channel, RoCE configuration on
  NVIDIA NICs.

## Related skills

- `networking-hardware-rdma-nic-offload` (this) pairs with
  `ebpf-verifier-reasoning` for kernel-visible network paths; packet I/O
  questions may also touch DPDK (documented, no skill yet).
- `kernel-container-internals` — namespaces/netns context when RDMA runs
  inside containers.
- `sanitizer-report-reading` — if the RDMA app itself is memory-unsafe.

## Evaluation

- Synthetic: bad fixtures must be caught — invented verbs API, illegal QP
  transition, atomic op without remote_atomic, send before RTS.
- False-positive: a correct RC lifecycle with all three modify_qp steps and
  matching access flags must pass; separate transport/link statements must
  not be flagged.
- Adversarial: an API that almost exists (`ibv_post_write` for RDMA_WRITE)
  and a "works without MR registration" claim must be rejected.
- Historical: no curated RDMA bug/CVE corpus is registered; the invented-API
  class is reproduced as a fixture (UNVERIFIED against upstream history).
- Researched gap: libibverbs/perftest/hardware absent; the host stand-in's
  real output is in `evals/README.md`.
