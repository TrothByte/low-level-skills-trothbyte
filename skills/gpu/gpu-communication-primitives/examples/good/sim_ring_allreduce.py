#!/usr/bin/env python3
"""Self-contained model of the ring all-reduce algorithm.

Models message passing between N ranks (each holding a vector of integers),
running the standard ring reduce-scatter + all-gather:

  reduce-scatter (step = 0..N-2):
    recv_chunk = (rank - step - 1) % N
    local[rank][recv_chunk] += local[(rank-1) % N][recv_chunk]   # ADD

  all-gather (step = 0..N-2):
    recv_chunk = (rank - step) % N
    local[rank][recv_chunk] = local[(rank-1) % N][recv_chunk]    # STORE

Each rank sends to (rank+1) % N and receives from (rank-1) % N; the "send" is
the read of local[(rank-1) % N][recv_chunk] above. After reduce-scatter, rank r
holds the fully-reduced chunk at position (r+1) % N; the all-gather then rotates
the reduced chunks so every rank holds every reduced chunk at its natural
position. This is the NCCL/MPI ring all-reduce semantics.

Verifies the *arithmetic and rotation semantics* for N=3,4,5,8 (including
non-power-of-two rank counts) — the counts where generated implementations break
(CommBench, arxiv-2608-04450). Models the collective, not GPU/NCCL hardware; no
nvcc/NCCL on this machine (documented target: nvcc -arch=sm_80 -lnccl).
"""

import random
import sys


def ground_truth(buffers):
    """Element-wise sum over all ranks; identical result on every rank."""
    n = len(buffers)
    m = len(buffers[0])
    return [[sum(buf[i] for buf in buffers) for i in range(m)] for _ in range(n)]


def ring_allreduce(buffers):
    n = len(buffers)
    m = len(buffers[0])
    chunk = m // n
    local = [list(buf) for buf in buffers]

    for step in range(n - 1):
        for rank in range(n):
            recv_chunk = (rank - step - 1) % n
            for i in range(chunk):
                j = recv_chunk * chunk + i
                local[rank][j] += local[(rank - 1) % n][j]

    for step in range(n - 1):
        for rank in range(n):
            recv_chunk = (rank - step) % n
            for i in range(chunk):
                j = recv_chunk * chunk + i
                local[rank][j] = local[(rank - 1) % n][j]

    return local


def ring_allreduce_wrong(buffers):
    """Intentionally incorrect: the all-gather reuses the reduce-scatter chunk
    formula (rank - step - 1) % N instead of (rank - step) % N. The reduced
    chunks are stored one position off -> correctly-shaped but permuted result.
    CommBench-style rotation bug."""
    n = len(buffers)
    m = len(buffers[0])
    chunk = m // n
    local = [list(buf) for buf in buffers]

    for step in range(n - 1):
        for rank in range(n):
            recv_chunk = (rank - step - 1) % n
            for i in range(chunk):
                j = recv_chunk * chunk + i
                local[rank][j] += local[(rank - 1) % n][j]

    for step in range(n - 1):
        for rank in range(n):
            recv_chunk = (rank - step - 1) % n  # BUG: should be (rank - step) % n
            for i in range(chunk):
                j = recv_chunk * chunk + i
                local[rank][j] = local[(rank - 1) % n][j]

    return local


def main():
    rng = random.Random(1234)
    ok = True
    for n in (3, 4, 5, 8):
        m = n * 3  # 3 chunks per rank
        buffers = [[rng.randint(0, 9) for _ in range(m)] for _ in range(n)]
        expected = ground_truth(buffers)
        got = ring_allreduce(buffers)
        wrong = ring_allreduce_wrong(buffers)

        correct = got == expected
        buggy = wrong != expected
        if not correct or not buggy:
            ok = False
        print(f"N={n}: correct rotation {'PASS' if correct else 'FAIL'}; "
              f"off-by-one rotation {'detected' if buggy else 'NOT detected'}")

        if not correct:
            for r in range(n):
                for i in range(m):
                    if got[r][i] != expected[r][i]:
                        print(f"  mismatch rank {r} idx {i}: got {got[r][i]} "
                              f"expected {expected[r][i]}")
                        break

    print("ring all-reduce model:", "ALL PASS" if ok else "FAILURES")
    print("Model of collective arithmetic and rotation semantics — not GPU "
          "hardware. Documented target: nvcc -arch=sm_80 -lnccl on multi-GPU.")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
