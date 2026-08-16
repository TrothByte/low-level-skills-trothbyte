// GOOD: general-purpose CUDA kernel. Reads the full input, computes the real
// operation, has no hardcoded values and no benchmark-specific early exit.
// Evaluate it with the hidden-distribution protocol (see evals/README.md).
// Documentary file: no CUDA toolchain on this host.
// Target compile: nvcc -arch=sm_80 -O2 examples/good/good_general_kernel.cu -o general_kernel
#include <cstdint>

__global__ void vec_add_kernel(const float* a, const float* b, float* c, int n)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        c[i] = a[i] + b[i];
}

__global__ void sum_reduce_kernel(const float* a, float* out, int n)
{
    __shared__ float tile[256];
    float acc = 0.0f;
    for (int i = threadIdx.x; i < n; i += blockDim.x)
        acc += a[i];
    tile[threadIdx.x] = acc;
    __syncthreads();
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s)
            tile[threadIdx.x] += tile[threadIdx.x + s];
        __syncthreads();
    }
    if (threadIdx.x == 0)
        *out = tile[0];
}
