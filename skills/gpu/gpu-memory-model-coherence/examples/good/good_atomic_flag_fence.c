/* GOOD: message passing with __threadfence() + relaxed atomics.
   This is the pattern from CUDA C++ Programming Guide §14.5.3.3 / §7.5.
   The producer writes the data, executes __threadfence() (a device-scope
   seq-cst fence), then signals with atomicExch. The consumer spins with a
   relaxed atomic read, executes __threadfence(), then reads the data.
   The fences on BOTH sides create the ordering that a bare relaxed atomic
   cannot. (The guide's alternative is cuda::atomic with acquire/release.) */

#include "../cuda_stubs.h"

#define CONSUMER_TID 1u

__device__ void good_atomic_flag_fence(int *data, unsigned int *flag, int *out)
{
    if (threadIdx.x == 0u) {
        /* Producer: publish data, fence, then signal. */
        *data = 42;
        GMC_THREADFENCE();              /* device-scope seq-cst fence */
        GMC_ATOMIC_EXCH(flag, 1u);      /* relaxed RMW signal */
    } else if (threadIdx.x == CONSUMER_TID) {
        /* Consumer: spin on the flag, fence, then read data. */
        while (GMC_ATOMIC_ADD(flag, 0u) == 0u) { (void)0; }
        GMC_THREADFENCE();              /* device-scope seq-cst fence */
        *out = *data;                   /* now ordered: data is visible */
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
    good_atomic_flag_fence(&data, &flag, &out);
    gmc_threadIdx.x = CONSUMER_TID;
    good_atomic_flag_fence(&data, &flag, &out);

    return (out == 42) ? 0 : 1;
}
