#ifndef GPU_MEMORY_MODEL_COHERENCE_CUDA_STUBS_H
#define GPU_MEMORY_MODEL_COHERENCE_CUDA_STUBS_H

/* Host-side stubs that let the CUDA-style kernel examples compile and run as plain
   C with GCC/Clang. Under a real CUDA toolchain (__CUDACC__) the real intrinsics are
   used instead. The host run is a SERIAL simulation: it checks compilation and kernel
   logic only. It never proves race freedom -- races are a GPU phenomenon that requires
   compute-sanitizer on real hardware. Teaching text is in the example files. */

#include <stddef.h>

#ifdef __CUDACC__
#define GMC_SYNCTHREADS()   __syncthreads()
#define GMC_THREADFENCE()   __threadfence()
#define GMC_ATOMIC_ADD(p, v)  atomicAdd((p), (v))
#define GMC_ATOMIC_EXCH(p, v) atomicExch((p), (v))
#define GMC_ATOMIC_INC(p, v)  atomicInc((p), (v))
#else
#define GMC_SYNCTHREADS()  ((void)0)
#define GMC_THREADFENCE()  ((void)0)

static inline unsigned int gmc_atomic_add(unsigned int *p, unsigned int v)
{
    unsigned int old = *p;
    *p = old + v;
    return old;
}

static inline unsigned int gmc_atomic_exch(unsigned int *p, unsigned int v)
{
    unsigned int old = *p;
    *p = v;
    return old;
}

static inline unsigned int gmc_atomic_inc(unsigned int *p, unsigned int v)
{
    unsigned int old = *p;
    *p = (old >= v) ? 0u : old + 1u;
    return old;
}

#define GMC_ATOMIC_ADD(p, v)  gmc_atomic_add((p), (v))
#define GMC_ATOMIC_EXCH(p, v) gmc_atomic_exch((p), (v))
#define GMC_ATOMIC_INC(p, v)  gmc_atomic_inc((p), (v))
#endif

typedef struct { unsigned int x, y, z; } gmc_dim3;

extern gmc_dim3 gmc_threadIdx, gmc_blockIdx, gmc_blockDim, gmc_gridDim;

#define threadIdx   gmc_threadIdx
#define blockIdx    gmc_blockIdx
#define blockDim    gmc_blockDim
#define gridDim     gmc_gridDim

#define __global__
#define __device__
#define __shared__
#define __constant__

#endif
