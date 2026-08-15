// intentionally incorrect — this is a BAD example (floor-grid tail-drop bug).
//
// The kernel computes out[i] = in[i] * 2, but the launch grid is floor(N/BLOCK)
// instead of ceil(N/BLOCK). For N that is a multiple of BLOCK the kernel looks
// correct; for any other N the last partial block never runs, so the tail
// elements are never written. This is the ISO-Bench "passes review but
// segfaults/breaks under load" failure class (arxiv-2602-19594): an agent that
// tests only power-of-two shapes will certify this.
//
// Compare with the fixed version in examples/good/good_ceil_grid.cu.
//
// Target toolchain: nvcc -arch=sm_80 (not available on this machine).

#include <stdio.h>
#include <stdlib.h>

#define BLOCK 256

__global__ void bad_floor_grid(const float *in, float *out, int N)
{
    int i = blockIdx.x * BLOCK + threadIdx.x;
    if (i < N)
        out[i] = in[i] * 2.0f;   // tail elements never written when N%BLOCK != 0
}

int main(void)
{
    const int N = 1000;              // NOT a multiple of BLOCK
    float *d_in, *d_out;
    float *h_out;
    size_t bytes = (size_t)N * sizeof(float);
    int i;

    h_out = (float *)malloc(bytes);
    for (i = 0; i < N; ++i) h_out[i] = -1.0f;   // sentinel

    cudaMalloc(&d_in, bytes);
    cudaMalloc(&d_out, bytes);
    // BUG: grid = N / BLOCK (floor) instead of (N + BLOCK - 1) / BLOCK.
    bad_floor_grid<<<N / BLOCK, BLOCK>>>(d_in, d_out, N);
    cudaMemcpy(h_out, d_out, bytes, cudaMemcpyDeviceToHost);

    // Elements [768, 1000) were never written: sentinel -1 survives.
    printf("out[999] == %f (expected 2*in[999]); -1 means unwritten tail\n",
           h_out[999]);

    printf("Researched — toolchain not available; command: "
           "nvcc -arch=sm_80 bad_floor_grid.cu\n");
    return 0;
}
