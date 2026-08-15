// intentionally incorrect — BAD example: all-gather output layout is not in
// rank order.
//
// ncclAllGather output is defined to be rank-order contiguous segments:
// rank0's buffer, then rank1's, ... The code below permutes the destination by
// (rank + i) % N, which produces a buffer with the right bytes in the wrong
// order. A reviewer who only checks shapes/totals misses it; the model trained
// on this data silently shuffles its expert routing.
//
// Target: nvcc -arch=sm_80 -lnccl on a multi-GPU host (not available here).

#include <nccl.h>
#include <stdio.h>
#include <stdlib.h>

#define N 3
#define COUNT 2

static void CHK(ncclResult_t rc)
{
    if (rc != ncclSuccess) {
        fprintf(stderr, "nccl error %d\n", rc);
        exit(1);
    }
}

int main(void)
{
    // Per-rank send buffer: rank r sends {r*100, r*100+1}.
    // Correct all-gather output must be: 0,1,100,101,200,201.
    float h_send[N][COUNT] = {{0, 1}, {100, 101}, {200, 201}};
    float h_recv[N * COUNT] = {0};
    float *d_send = NULL, *d_recv = NULL;
    ncclComm_t comm;
    cudaStream_t stream;
    int rank, i, k;

    CHK(ncclCommInitRank(&comm, N, ncclUniqueId{0}, rank /* per-rank */));
    cudaStreamCreate(&stream);
    cudaMalloc(&d_send, N * COUNT * sizeof(float));
    cudaMalloc(&d_recv, N * COUNT * sizeof(float));
    cudaMemcpy(d_send, h_send, sizeof(h_send), cudaMemcpyHostToDevice);

    // BUG: destination index (rank + i) % N instead of (rank * COUNT + i).
    // The bytes are copied but the segment order is rotated.
    for (i = 0; i < COUNT; ++i) {
        int dst = ((rank + i) % N) * COUNT + i;
        CHK(ncclAllGather(d_send + rank * COUNT, d_recv + dst, 1,
                          ncclFloat, comm, stream));
    }
    cudaStreamSynchronize(stream);
    cudaMemcpy(h_recv, d_recv, sizeof(h_recv), cudaMemcpyDeviceToHost);

    printf("output: ");
    for (k = 0; k < N * COUNT; ++k) printf("%.0f ", h_recv[k]);
    printf("\nexpected: 0 1 100 101 200 201 (rank-order contiguous)\n");
    printf("Researched — toolchain not available; command: "
           "nvcc -arch=sm_80 -lnccl bad_allgather_layout.cu\n");
    return 0;
}
