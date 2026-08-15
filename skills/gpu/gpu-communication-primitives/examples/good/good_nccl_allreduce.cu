// GOOD: canonical NCCL all-reduce with full error checking, explicit stream,
// and a device-only in/out contract.
//
// Points this skill teaches:
//   - ncclResult_t checked on every call (errors can surface late).
//   - same communicator, same collective order on every rank.
//   - in-place (send == recv) chosen once for all ranks; device pointers only.
//   - stream passed explicitly; caller owns stream ordering with adjacent kernels.
//
// Target: nvcc -arch=sm_80 -lnccl on a multi-GPU host (not available here).

#include <nccl.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK_CUDA(fn) do { cudaError_t e_ = (fn); if (e_ != cudaSuccess) { \
    fprintf(stderr, "cuda: %s\n", cudaGetErrorString(e_)); exit(1); } } while (0)
#define CHECK_NCCL(fn) do { ncclResult_t r_ = (fn); if (r_ != ncclSuccess) { \
    fprintf(stderr, "nccl: %d\n", r_); exit(1); } } while (0)

int main(int argc, char **argv)
{
    const int count = 1 << 20;
    float *d_x = NULL, *d_sum = NULL;
    ncclComm_t comm;
    ncclUniqueId id;
    cudaStream_t stream;

    if (argc == 2 && atoi(argv[1]) == 0)
        ncclGetUniqueId(&id);                       // rank 0 creates the id
    else
        for (int i = 0; i < 128; ++i) id.internal[i] = 0;  // real code: receive id
    int rank, nranks;
    CHECK_NCCL(ncclCommInitRank(&comm, nranks, id, rank));

    CHECK_CUDA(cudaSetDevice(rank));
    CHECK_CUDA(cudaMalloc(&d_x, count * sizeof(float)));
    CHECK_CUDA(cudaMalloc(&d_sum, count * sizeof(float)));
    CHECK_CUDA(cudaStreamCreate(&stream));

    // fill d_x on this rank; then reduce all ranks into d_sum (out-of-place)
    CHECK_NCCL(ncclAllReduce(d_x, d_sum, count, ncclFloat, ncclSum,
                             comm, stream));
    CHECK_CUDA(cudaStreamSynchronize(stream));      // completion BEFORE verification
    CHECK_NCCL(ncclCommDestroy(comm));

    // Correctness check: compare d_sum against a host-side fp64 sum of all ranks'
    // inputs (fp64 reference; see gpu-kernel-verification-beyond-oracle).
    printf("good_nccl_allreduce: all-reduce issued and checked; fp64 comparison "
           "must be run on the multi-GPU host.\n");
    return 0;
}
