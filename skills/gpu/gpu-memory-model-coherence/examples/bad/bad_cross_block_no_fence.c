/* BAD: cross-block visibility without __threadfence().
   Each block stores its partial sum to global memory and then atomically
   increments a counter. The last block (the one that sees old == GRID_SIZE-1)
   reads all partial sums. Without a fence between the partial store and the
   counter increment, the counter can reach gridDim-1 BEFORE the partial sums
   are visible device-wide, so the last block reads stale data.
   This is the pattern from CUDA C++ Programming Guide §7.5, with the fence
   intentionally removed. */

#include "../cuda_stubs.h"

#define GRID_SIZE 4u
#define N 8u

__global__ void bad_cross_block_no_fence(const int *in, int *partials,
                                         unsigned int *count, int *total)
{
    unsigned int b = blockIdx.x;
    unsigned int i;
    int sum = 0;

    for (i = b; i < N; i += GRID_SIZE) {
        sum += in[i];
    }

    partials[b] = sum;
    /* BUG: no __threadfence() here. The increment may be observed by the
       last block before the partial store above is visible device-wide. */
    unsigned int old = GMC_ATOMIC_INC(count, GRID_SIZE);

    if (old == GRID_SIZE - 1u) {
        /* Last block: sum the partials written by all blocks. */
        int acc = 0;
        for (i = 0u; i < GRID_SIZE; ++i) {
            acc += partials[i];   /* may read stale partials on the GPU */
        }
        *total = acc;
    }
}

gmc_dim3 gmc_threadIdx, gmc_blockIdx, gmc_blockDim, gmc_gridDim;

int main(void)
{
    const int in[N] = {1, 2, 3, 4, 5, 6, 7, 8};
    int partials[GRID_SIZE] = {0};
    unsigned int count = 0u;
    int total = 0;
    unsigned int b;

    gmc_gridDim.x = GRID_SIZE;
    gmc_blockDim.x = 1u;
    gmc_threadIdx.x = 0u;

    for (b = 0u; b < GRID_SIZE; ++b) {
        gmc_blockIdx.x = b;
        bad_cross_block_no_fence(in, partials, &count, &total);
    }

    /* Serial host simulation always orders the partial stores before the last
       block, so the host "passes". On the GPU the missing fence makes the
       last block read stale partials -- a nondeterministic failure. */
    return (total == 36) ? 0 : 1;
}
