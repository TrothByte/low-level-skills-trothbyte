// intentionally incorrect — this is a BAD example (fixed-shape oracle trap).
//
// A "verification" harness that runs the kernel at ONE fixed shape (1024,
// a multiple of the block size) and certifies it with a float32 allclose
// comparison against a float32 reference. Both sides are computed in fp32,
// so any precision drift is invisible, and the floor-grid bug in the kernel
// never triggers because 1024/256 divides evenly.
//
// The Correctness Illusion result (arxiv-2606-20128) is exactly this: such
// oracles certify buggy kernels; fuzz + fp64 reference caught 9/9 of them.
// This file is the anti-pattern. The corrected pipeline lives in
// examples/good/sim_oracle_weakness.py.
//
// Target toolchain: nvcc -arch=sm_80 (not available on this machine).

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#define BLOCK 256

__global__ void flawed_kernel(const float *in, float *out, int N)
{
    // BUG: grid is floor(N/BLOCK), so when N is not a multiple of BLOCK
    // the last partial block never executes and out[] keeps garbage for
    // the tail elements. Hidden because the harness only tests N=1024.
    int i = blockIdx.x * BLOCK + threadIdx.x;
    if (i < N)
        out[i] = in[i] * 2.0f;
}

int main(void)
{
    const int N = 1024;                 // only shape ever tested
    float *d_in, *d_out;
    float *h_in, *h_out, *h_ref;
    size_t bytes = (size_t)N * sizeof(float);
    int i, ok;

    h_in  = (float *)malloc(bytes);
    h_out = (float *)malloc(bytes);
    h_ref = (float *)malloc(bytes);
    for (i = 0; i < N; ++i) h_in[i] = (float)i;

    cudaMalloc(&d_in, bytes);
    cudaMalloc(&d_out, bytes);
    cudaMemcpy(d_in, h_in, bytes, cudaMemcpyHostToDevice);

    flawed_kernel<<<N / BLOCK, BLOCK>>>(d_in, d_out, N);
    cudaMemcpy(h_out, d_out, bytes, cudaMemcpyDeviceToHost);

    // float32 reference -- same precision as the kernel, so it hides drift.
    for (i = 0; i < N; ++i) h_ref[i] = h_in[i] * 2.0f;

    ok = 1;
    for (i = 0; i < N; ++i)
        if (fabsf(h_out[i] - h_ref[i]) > 1e-5f * fabsf(h_ref[i])) ok = 0;

    printf("fixed-shape fp32 oracle: %s\n", ok ? "PASS" : "FAIL");
    printf("BUG: a single shape at a multiple of BLOCK cannot certify the kernel.\n");
    printf("Researched — toolchain not available; command: "
           "nvcc -arch=sm_80 bad_fixed_shape_oracle.cu\n");
    return 0;
}
