/* BAD: shared-memory race -- neighbor's shared write read without a barrier.
   Each thread stores its input to shared memory, then reads the PREVIOUS
   thread's slot. Without __syncthreads() the read can happen before the
   neighbor's store is visible to this thread; the shared store is only
   guaranteed visible block-wide after a barrier.
   On the GPU this produces nondeterministic garbage in out[]. The host run
   below is a serial simulation: it checks compilation, not race freedom. */

#include "../cuda_stubs.h"

#define TPB 4u

__shared__ int sh[TPB];

__global__ void bad_shared_no_syncthreads(const int *in, int *out)
{
    unsigned int t = threadIdx.x;

    sh[t] = in[t];
    /* BUG: no __syncthreads() between the store above and the read below.
       Thread 0 can read sh[3] before thread 3 has stored it. */
    int r = sh[(t + TPB - 1u) % TPB];
    out[t] = r;
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

    for (t = 0u; t < TPB; ++t) {
        gmc_threadIdx.x = t;
        bad_shared_no_syncthreads(in, out);
    }

    /* Serial host simulation: thread 0 reads sh[3] before it was written
       (still the zero-initialized value), so out[0] == 0 instead of 40.
       On the GPU the failure is nondeterministic instead of deterministic. */
    return (out[0] == 40 && out[1] == 10 && out[2] == 20 && out[3] == 30) ? 0 : 1;
}
