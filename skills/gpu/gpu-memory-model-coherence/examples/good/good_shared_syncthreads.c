/* GOOD: shared-memory exchange with __syncthreads().
   The kernel is split into two phases so the host can model the block:
   phase 0 = every thread stores its input and hits the block barrier;
   phase 1 = every thread reads its neighbor's slot.
   __syncthreads() waits until every non-exited thread of the block has arrived
   AND makes all global and shared accesses made before it visible to all
   threads of the block. It must be reached by the whole block (here the phase
   condition is identical for all threads, so the barrier is legal). */

#include "../cuda_stubs.h"

#define TPB 4u

__shared__ int sh[TPB];

__global__ void good_shared_syncthreads(const int *in, int *out, unsigned int phase)
{
    unsigned int t = threadIdx.x;

    if (phase == 0u) {
        sh[t] = in[t];
        GMC_SYNCTHREADS();   /* every thread arrives; stores now visible */
    } else {
        out[t] = sh[(t + TPB - 1u) % TPB];
    }
}

gmc_dim3 gmc_threadIdx, gmc_blockIdx, gmc_blockDim, gmc_gridDim;

int main(void)
{
    const int in[TPB] = {10, 20, 30, 40};
    int out[TPB] = {0};
    unsigned int t;

    gmc_gridDim.x = 1u;
    gmc_blockDim.x = TPB;
    gmc_blockIdx.x = 0u;

    /* Phase 0: all threads write. */
    for (t = 0u; t < TPB; ++t) {
        gmc_threadIdx.x = t;
        good_shared_syncthreads(in, out, 0u);
    }
    /* Phase 1: all threads read. On the GPU the barrier between the phases
       guarantees the writes are visible; the host serial order models it. */
    for (t = 0u; t < TPB; ++t) {
        gmc_threadIdx.x = t;
        good_shared_syncthreads(in, out, 1u);
    }

    /* Correct cyclic shift: out[t] == in[(t + 3) % 4]. */
    return (out[0] == 40 && out[1] == 10 && out[2] == 20 && out[3] == 30) ? 0 : 1;
}
