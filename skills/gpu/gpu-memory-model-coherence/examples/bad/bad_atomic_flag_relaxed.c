/* BAD: relaxed atomic used as a synchronization flag.
   CUDA atomic functions (atomicAdd, atomicExch, ...) have memory_order_relaxed
   ordering: atomicity on the counter only, NO happens-before on the data.
   The consumer can observe flag != 0 before the producer's data store is
   visible, so *out may be stale. Correct versions pair the relaxed atomic with
   __threadfence() on both sides (see good_atomic_flag_fence.c) or use
   cuda::atomic with acquire/release. */

#include "../cuda_stubs.h"

#define CONSUMER_TID 1u

__device__ void bad_atomic_flag_relaxed(int *data, unsigned int *flag, int *out)
{
    if (threadIdx.x == 0u) {
        /* Producer: writes data, then signals. */
        *data = 42;
        GMC_ATOMIC_ADD(flag, 1u);   /* BUG: relaxed RMW orders nothing */
    } else if (threadIdx.x == CONSUMER_TID) {
        /* Consumer: spins on the flag, then reads data. */
        while (GMC_ATOMIC_ADD(flag, 0u) == 0u) { (void)0; }
        *out = *data;   /* BUG: no happens-before edge; may read stale data */
    }
}

gmc_dim3 gmc_threadIdx, gmc_blockIdx, gmc_blockDim, gmc_gridDim;

int main(void)
{
    int data = 0;
    unsigned int flag = 0u;
    int out = 0;

    gmc_gridDim.x = 1u;
    gmc_blockDim.x = 2u;
    gmc_blockIdx.x = 0u;
    gmc_threadIdx.x = 0u;
    bad_atomic_flag_relaxed(&data, &flag, &out);
    gmc_threadIdx.x = CONSUMER_TID;
    bad_atomic_flag_relaxed(&data, &flag, &out);

    /* On the host, serial execution makes this "work". On the GPU the relaxed
       flag creates no ordering and the data read races. */
    return (out == 42) ? 0 : 1;
}
