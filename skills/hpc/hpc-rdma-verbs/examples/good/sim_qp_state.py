#!/usr/bin/env python3
"""Self-contained model of the RDMA QP state machine and WR/completion rules.

Models the libibverbs semantics (rdma-verbs-docs / libibverbs):
- QP legal transitions: RESET->INIT->RTR->RTS, any->ERR, ERR->RESET.
- ibv_post_send requires RTS; ibv_post_recv requires >= INIT.
- RDMA WRs require remote_addr + rkey; SEND requires only the local sge.
- A completion is polled from the CQ; status must be SUCCESS.

Checks the good/bad patterns from the examples as a deterministic oracle.
This models the API contract, not the HCA. No RDMA hardware/libibverbs on this
machine (documented target: gcc -libverbs on a host with mlx4/mlx5).
"""

QPS = {"RESET", "INIT", "RTR", "RTS", "ERR"}
TRANSITIONS = {
    ("RESET", "INIT"), ("INIT", "RTR"), ("RTR", "RTS"),
    ("RTS", "ERR"), ("RTR", "ERR"), ("INIT", "ERR"), ("RESET", "ERR"),
    ("ERR", "RESET"),
}


class QP:
    def __init__(self, qp_type="RC"):
        self.state = "RESET"
        self.qp_type = qp_type

    def modify(self, to):
        if (self.state, to) not in TRANSITIONS:
            return f"EINVAL: illegal transition {self.state}->{to}"
        self.state = to
        return None

    def post_send(self, rdma=False, rkey=None):
        if self.state != "RTS":
            return f"error: post_send in state {self.state} (RTS required)"
        if rdma and rkey is None:
            return "error: RDMA WR missing rkey"
        return None

    def post_recv(self):
        if self.state in ("RESET",):
            return f"error: post_recv in state {self.state} (INIT+ required)"
        return None


def main():
    ok = True
    print("RDMA QP state machine model\n")

    # bad_post_send_state: post before RTS
    q = QP()
    r = q.post_send()
    print(f"bad_post_send_state: post_send in {q.state}: {r}")
    ok &= r is not None

    # legal full sequence
    q = QP()
    seq = []
    for s in ("INIT", "RTR", "RTS"):
        e = q.modify(s)
        if e: seq.append(e)
    r = q.post_send()
    seq.append(r)
    print(f"good_qp_setup: RESET->INIT->RTR->RTS then post_send: "
          f"{'OK' if r is None else r}")
    ok &= r is None

    # bad_missing_rkey: RDMA WR without rkey
    q = QP()
    for s in ("INIT", "RTR", "RTS"):
        q.modify(s)
    r = q.post_send(rdma=True, rkey=None)
    print(f"bad_missing_rkey: RDMA_WRITE without rkey: {r}")
    ok &= r is not None

    # good_rdma_write: RDMA WR with rkey
    r = q.post_send(rdma=True, rkey=0x1234)
    print(f"good_rdma_write: RDMA_WRITE with rkey: "
          f"{'OK' if r is None else r}")
    ok &= r is None

    # illegal transition
    q = QP()
    e = q.modify("RTS")  # RESET->RTS illegal
    print(f"illegal RESET->RTS: {e}")
    ok &= e is not None

    # post_recv at RESET
    q = QP()
    r = q.post_recv()
    print(f"post_recv at RESET: {r}")
    ok &= r is not None

    print("\nAll model checks:", "PASS" if ok else "FAIL")
    print("Model of libibverbs API contract — not HCA hardware. Documented "
          "target: gcc -libverbs on an RDMA host.")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
