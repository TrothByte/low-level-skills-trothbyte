#!/usr/bin/env python3
"""Model of MPI blocking-send / blocking-recv rendezvous semantics.

Models the message-passing semantics of MPI_Send/MPI_Recv (mpi-41 §3.4-3.7):

- A blocking Send delivers only when the peer has posted a matching receive
  slot (rendezvous: large messages are not buffered); otherwise the send stays
  pending and the sending rank blocks.
- A blocking Recv waits for the peer's send to deliver into its slot.
- Posting Irecv FIRST makes the slot available, so the peer's Isend delivers
  immediately — the deadlock-free reordering.

Demonstrates: two ranks that each block on Send before posting Recv deadlock;
the same exchange with Irecv-before-Isend completes.

This models MPI ordering/buffering semantics, not the runtime internals. No
mpicc/mpirun on this machine (documented target: mpicc && mpirun -np 2).
"""

import sys


def exchange(recv_first):
    """Two-rank exchange. recv_first=True posts Irecv before Isend."""
    posted_recv = {0: False, 1: False}    # peer's receive slot available
    send_done = {0: False, 1: False}      # this rank's send has delivered
    received = {0: False, 1: False}       # this rank has received the peer's data

    if recv_first:
        posted_recv[0] = posted_recv[1] = True

    for step in range(100):
        progressed = False

        # phase 1: try to issue (and deliver) each rank's send
        for r in (0, 1):
            if send_done[r]:
                continue
            if posted_recv[1 - r]:
                received[1 - r] = True     # peer's slot open -> deliver now
                send_done[r] = True
                progressed = True
            # else: rendezvous — the send stays pending, rank r blocks

        if received[0] and received[1]:
            return "COMPLETE"
        if not progressed:
            return "DEADLOCK"
    return "INCOMPLETE"


def main():
    print("MPI blocking-send semantics model (mpi-41 §3.4)\n")

    res_bad = exchange(recv_first=False)   # bad_send_send pattern
    res_good = exchange(recv_first=True)   # good_nonblocking pattern
    print(f"blocking Send/Send, no pre-posted Recv: {res_bad}")
    print(f"Irecv first, then Isend+Wait:            {res_good}")

    ok = res_bad == "DEADLOCK" and res_good == "COMPLETE"
    print("\nModel:", "PASS (deadlock reproduced, reorder fixes it)" if ok
          else "FAIL")
    print("Model of MPI ordering semantics — not the runtime. Documented target: "
          "mpicc && mpirun -np 2.")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
