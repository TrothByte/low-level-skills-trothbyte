# Evaluation — networking-hardware-rdma-nic-offload

Skill: `skills/networking/networking-hardware-rdma-nic-offload`.
Toolchain status: RESEARCHED. No RDMA NIC, no libibverbs, no perftest, no
DOCA SDK on this host. A host-side python stand-in
(`examples/check_verbs_trace.py`) models the QP state machine and MR access
rules and was actually run; hardware/verb compile checks are documented only.

## Synthetic evals (host stand-in, run 2026-08-15)

Command: `python examples/check_verbs_trace.py good/qp_rc_trace.txt bad/qp_bad_transition.txt bad/rdma_atomic_no_access.txt bad/post_before_rts.txt`

Recorded output (exit 1 overall):

```
PASS .../good/qp_rc_trace.txt
FAIL .../bad/qp_bad_transition.txt:4: illegal QP transition RESET->RTS
FAIL .../bad/qp_bad_transition.txt:5: post_send send with QP in RESET (needs RTS)
FAIL .../bad/rdma_atomic_no_access.txt:10: atomic_fetch_add requires MR
    'remote_atomic' (flags: ['local_write'])
FAIL .../bad/post_before_rts.txt:5: post_send send with QP in RESET (needs RTS)
```

## Researched evals (toolchain absent — exact commands, NOT run)

| Case | Fixture | Expected | Command |
|---|---|---|---|
| invented verbs API | `bad/verbs_invented_api.c` | compile error: `ibv_create_qpair`/`ibv_post_write` unknown | `gcc -libverbs -c bad/verbs_invented_api.c` |
| correct RDMA_READ | `good/verbs_rdma_read_sketch.c` | compiles against infiniband/verbs.h | `gcc -libverbs -c good/verbs_rdma_read_sketch.c` |
| hardware check | — | QP reaches RTS, RDMA_READ completes | `ib_write_bw` / `ibv_devinfo -v` |

Honest status: the C fixtures use the real `<infiniband/verbs.h>` API but
were not compiled (libibverbs absent); the hardware/perftest rows are
documented research.

## Verified facts (ACTUAL on this host)

- `check_verbs_trace.py` runs under python 3.11.9 and correctly accepts the
  good RC lifecycle and rejects the three bad traces with the recorded
  messages above (real output, this host).
- The QP-state and MR-access rules encoded in the stand-in are KNOWN from
  the libibverbs man pages and RDMA docs (see `references/`); their
  enforcement on actual NIC hardware is UNVERIFIED here.

## False-positive evals (researched)

- A correct RC program with INIT/RTR/RTS and `IBV_ACCESS_REMOTE_READ` for an
  RDMA_READ must pass.
- Statements that separate transport (RC) from link (RoCEv2) must pass.
- `ethtool -k`/`ibv_devinfo`-style queries as the verification method must
  not be flagged as "not doing anything".

## Adversarial evals (researched)

- `ibv_post_write` presented as the RDMA_WRITE API must be caught by the
  header check.
- A claim that fetch-and-add works without remote_atomic must be caught by
  the access-flag rule.
- A "RoCE needs no L2 config" claim must be flagged by the transport/link
  rule.

## Historical evals

- No curated RDMA bug/CVE corpus is registered in this repository. The
  invented-API class is reproduced as a fixture; the real-world equivalents
  are known failure reports in the RDMA community (UNVERIFIED here).

## Target toolchains (absent, documented)

- libibverbs + `ibv_devinfo`/`ibv_pingpong`/perftest: not present.
- RDMA hardware or soft-RoCE (siw) kernel module: not present.
- DOCA SDK: not present. Planned elevation: a Linux host with an
  RDMA-capable NIC or soft-RoCE, then compile the good C fixture and run
  perftest.
