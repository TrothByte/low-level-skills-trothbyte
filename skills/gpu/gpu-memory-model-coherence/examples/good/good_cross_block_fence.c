/* GOOD: cross-block last-block detection with __threadfence().
   Mirrors the CUDA C++ Programming Guide §7.5 example. Each block stores its
   partial sum, executes __threadfence() (device-scope seq-cst fence) so the
   store is ordered before the counter increment, then atomically increments
   the counter. The last block (old == GRID_SIZE-1) then reads all partials --
   the fence guarantees they are visible device-wide. The partials array is
   volatile so the stores bypass L1 and are not cached away from the last
   block. */

#include "../cuda_stubs.h"

#define GRID_SIZE 4u
#define N 8u

__global__ void good_cross_block_fence(const int *in, volatile int *partials,
                                       unsigned int *count, int *total)
{
    unsigned int b = blockIdx.x;
    unsigned int i;
    int sum = 0;

    for (i = b; i < N; i += GRID_SIZE) {
        sum += in[i];
    }

    partials[b] = sum;
    GMC_THREADFENCE();   /* order the partial store before the signal */
    unsigned int old = GMC_ATOMIC_INC(count, GRID_SIZE);

    if (old == GRID_SIZE - 1u) {
        int acc = 0;
        for (i = 0u; i < GRID_SIZE; ++i) {
            acc += partials[i];   /* visible: fenced on the producer side */
        }
        *total = acc;
    }
}

gmc_dim3 gmc_threadIdx, gmc_blockIdx, gmc_blockDim, gmc_gridDim;

int main(void)
{
    const int in[N] = {1, 2, 3, 4, 5, 6, 7, 8};
    static volatile int partials[GRID_SIZE];
    unsigned int count = 0u;
    int total = 0;
    unsigned int b;

    gmc_gridDim.x = GRID_SIZE;
    gmc_blockDim.x = 1u;
    gmc_threadIdx.x = 0u;

    for (b = 0u; b < GRID_SIZE; ++b) {
        gmc_blockIdx.x = b;
        good_cross_block_fence(in, partials, &count, &total);
    }

    return (total == 36) ? 0 : 1;
}
