# Evaluation — hpc-rdma-verbs

Skill: `skills/hpc/hpc-rdma-verbs`.
Stability: `researched` (source-backed grounding: libibverbs, rdma-verbs-docs,
mpi-41). No RDMA hardware, no libibverbs on this machine (win32); the `.c`
examples are documentary with target commands recorded. The QP state-machine and
WR/rkey/completion rules were verified with a self-contained Python 3.11 model
(`examples/good/sim_qp_state.py`), actually run; output recorded below. Mark:
SIMULATED — models the libibverbs API contract, not HCA hardware.

## Toolchain status

`gcc -libverbs`, RDMA devices (mlx4/mlx5), perftest: NOT available. Consequences:

- `bad_*.c` and `good_*.c` compile only on an RDMA host. Target commands recorded
  in each file. NOT run here.
- The Python model verifies the QP legal transitions, post_send/post_recv state
  requirements, and the RDMA rkey requirement — the parts of the API contract
  that are pure logic. It does not model the HCA, completions timing, or RoCE
  packet loss.

Target commands to promote to `verified` (RDMA host):

```
gcc -Wall -Wextra -O2 -libverbs examples/good/good_qp_setup.c -o good_qp
./good_qp server 1        # IB: LID 1; RoCE: GID index
./good_qp client <peer>   # expect: clean QP sequence + SEND + completion
gcc -Wall -Wextra -O2 -libverbs examples/bad/bad_post_send_state.c -o bad_ps
./bad_ps                  # expect: ibv_post_send error (RESET QP)
perftest: ib_write_bw -d mlx5_0 -i 1 <server> ; ib_read_lat -d mlx5_0 -i 1 <server>
```

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/bad_post_send_state.c` | post_send before RTS | model-checked |
| medium/negative | `bad/bad_missing_rkey.c` | RDMA WR without rkey | model-checked |
| medium/negative | `bad/bad_cq_poll.c` | poll without arm/re-arm loop | review-time flag |
| medium/negative | `bad/bad_ud_reliable.c` | UD treated as reliable | review-time flag |
| positive | `good/good_qp_setup.c` | RESET->INIT->RTR->RTS + signaled SEND + completion | model-checked (state seq) |
| positive | `good/good_rdma_write.c` | RDMA_WRITE with rkey + poll with re-arm | model-checked |
| positive | `good/good_roce_gid.c` | link-layer-based addressing (LID vs GID) | review-time |

## False-positive evals (correct code must NOT be flagged)

- `good/good_qp_setup.c` — the four-step modify sequence with per-transition
  attribute masks: correct.
- `good/good_rdma_write.c` — remote addr + rkey set; MR deregistered only after
  the WR completes: correct.
- `good/good_roce_gid.c` — branching on `link_layer` for LID vs GID: correct;
  must NOT be "simplified" to a hard-coded LID.
- RC selected for reliable data (not UD): correct; must NOT be flagged as
  "overkill".

## Historical evals

Not applicable as dedicated category: no CVE is attributed to verbs usage in this
skill's scope. The failure classes (QP-state violations, rkey loss, completion
races) are documented from `rdma-verbs-docs` / `libibverbs`. A historical
RDMA-bug CVE corpus is out of scope until an RDMA host exists.

## Adversarial evals

- A program that works on InfiniBand loopback (LID hard-coded) but breaks on a
  RoCE fabric — the agent must demand link-layer-aware addressing.
- A completion race where the local MR/buffer is freed before the remote
  RDMA_WRITE completes — must be caught by the "keep MR alive until WC polled"
  rule.
- `bad/bad_cq_poll.c` — poll-once with no re-arm: the agent must identify the
  missed-completion hazard even though the single test run "completed".

## Verified facts (python 3.11.9 run, recorded 2026-08-15)

Command: `python examples/good/sim_qp_state.py`

```
RDMA QP state machine model

bad_post_send_state: post_send in RESET: error: post_send in state RESET (RTS required)
good_qp_setup: RESET->INIT->RTR->RTS then post_send: OK
bad_missing_rkey: RDMA_WRITE without rkey: error: RDMA WR missing rkey
good_rdma_write: RDMA_WRITE with rkey: OK
illegal RESET->RTS: EINVAL: illegal transition RESET->RTS
post_recv at RESET: error: post_recv in state RESET (INIT+ required)

All model checks: PASS
Model of libibverbs API contract — not HCA hardware. Documented target: gcc -libverbs on an RDMA host.
```

Interpretation: post_send is rejected below RTS and RDMA WRs without an rkey are
rejected; the full four-step sequence with an rkey succeeds; illegal transitions
and post_recv at RESET are caught. This is the API-contract part of
`references/rdma-verbs.md` rules 1-3, 7.

## Scoring (for routing eval)

- recall: QP-state violation, missing rkey, poll-without-arm, and UD-as-reliable
  detected via reference rules.
- precision: correct state sequences, rkey usage, and completion loops produce
  zero flags.
- FP-rate: zero expected on the good set.

## Target toolchains (absent, documented)

- `gcc -libverbs` on a host with RDMA hardware (mlx4/mlx5) or soft-RoCE.
- `perftest` suite (`ib_write_bw`, `ib_read_lat`): bandwidth/latency checks.
- Python 3.11 API-contract model: AVAILABLE, run, recorded above.
