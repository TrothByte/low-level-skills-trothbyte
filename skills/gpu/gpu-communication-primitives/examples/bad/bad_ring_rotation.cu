// intentionally incorrect — BAD example: off-by-one in the ring all-reduce
// all-gather phase.
//
// reduce-scatter uses recv_chunk = (rank - step - 1) % N (correct), but the
// all-gather uses recv_chunk = (rank - step) % N (off by one). The reduced
// chunks are stored at the wrong positions, producing a correctly-shaped
// buffer whose contents are permuted. A single-rank or power-of-two-N test
// may pass; N=3 or N=6 exposes it. This is the CommBench rotation-bug class
// (arxiv-2608-04450).
//
// Target: nvcc -arch=sm_80 -lnccl on a multi-GPU host (not available here).

#include <nccl.h>
#include <stdio.h>
#include <stdlib.h>

__global__ void noop(float *p, int n)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) p[i] = 1.0f;
}

static void CHK(int rank, ncclResult_t rc)
{
    if (rc != ncclSuccess) {
        fprintf(stderr, "rank %d: nccl error %d\n", rank, rc);
        exit(1);
    }
}

// Pure host-side bookkeeping of chunk indices to keep the example readable:
static int recv_chunk_correct(int rank, int step, int N) { return ((rank - step - 1) % N + N) % N; }
static int recv_chunk_buggy(int rank, int step, int N)   { return ((rank - step) % N + N) % N; }

int main(void)
{
    int N = 6, nbytes = 6 * sizeof(float);   // N ranks, 1 float per chunk
    int rank, step;

    // Demonstrate the off-by-one on the chunk index alone (host arithmetic).
    for (rank = 0; rank < N; ++rank)
        for (step = 0; step < N - 1; ++step) {
            int good = recv_chunk_correct(rank, step, N);
            int bad  = recv_chunk_buggy(rank, step, N);
            if (good != bad)
                printf("rank %d step %d: correct chunk %d, buggy chunk %d (WRONG)\n",
                       rank, step, good, bad);
        }

    printf("BUG: all-gather stores reduced chunks at permuted positions.\n");
    printf("Researched — toolchain not available; command: "
           "nvcc -arch=sm_80 -lnccl bad_ring_rotation.cu\n");
    return 0;
}
