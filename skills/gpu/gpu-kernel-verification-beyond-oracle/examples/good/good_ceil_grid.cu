// GOOD: ceil-grid kernel with an in-kernel tail guard — the pattern the
// oracle skill teaches. Every indexed access is guarded by i < N, and the
// launch uses ceil(N/BLOCK). Works for any N including 0 (host skips the
// launch for N == 0; the empty-input contract is "produce nothing").
//
// Verification discipline (see references/verification-oracles.md):
//   1. shape sweep: 0,1,2,127,128,255,256,257,511,512,513,1023,1024,1025,1<<20
//   2. fp64 reference, not fp32
//   3. compute-sanitizer --tool memcheck for OOB writes
//
// Target toolchain: nvcc -arch=sm_80 (not available on this machine).

#include <stdio.h>
#include <stdlib.h>

#define BLOCK 256

__global__ void good_ceil_grid(const float *in, float *out, int N)
{
    int i = blockIdx.x * BLOCK + threadIdx.x;
    if (i < N)
        out[i] = in[i] * 2.0f;
}

static int check_shape(int N)
{
    float *d_in = NULL, *d_out = NULL;
    float *h_out;
    double *h_ref;
    size_t bytes = (size_t)N * sizeof(float);
    int i, ok = 1;
    double atol = 1e-9, rtol = 1e-6;

    if (N == 0) return 1;              // empty-input contract: defined as success

    h_out = (float *)malloc(bytes);
    h_ref = (double *)malloc((size_t)N * sizeof(double));

    cudaMalloc(&d_in, bytes);
    cudaMalloc(&d_out, bytes);
    // fill device input deterministically on host, upload
    float *h_in = (float *)malloc(bytes);
    for (i = 0; i < N; ++i) h_in[i] = (float)(i % 97);
    cudaMemcpy(d_in, h_in, bytes, cudaMemcpyHostToDevice);

    // ceil, NOT floor:
    good_ceil_grid<<<(N + BLOCK - 1) / BLOCK, BLOCK>>>(d_in, d_out, N);
    cudaMemcpy(h_out, d_out, bytes, cudaMemcpyDeviceToHost);

    // fp64 reference, independent path:
    for (i = 0; i < N; ++i) h_ref[i] = 2.0 * h_in[i];
    for (i = 0; i < N; ++i)
        if (!(fabs((double)h_out[i] - h_ref[i]) <= atol + rtol * fabs(h_ref[i])))
            ok = 0;

    free(h_in);
    free(h_out);
    free(h_ref);
    cudaFree(d_in);
    cudaFree(d_out);
    return ok;
}

int main(void)
{
    int sizes[] = {0,1,2,127,128,255,256,257,511,512,513,
                   1023,1024,1025,4095,4096,1<<20};
    int i, fails = 0;
    for (i = 0; i < (int)(sizeof(sizes)/sizeof(sizes[0])); ++i)
        if (!check_shape(sizes[i])) { fails++; printf("FAIL shape %d\n", sizes[i]); }
    printf("good_ceil_grid: %d/%d shapes passed (fp64 reference)\n",
           (int)(sizeof(sizes)/sizeof(sizes[0])) - fails,
           (int)(sizeof(sizes)/sizeof(sizes[0])));
    printf("Researched — toolchain not available; command: "
           "nvcc -arch=sm_80 good_ceil_grid.cu && "
           "compute-sanitizer --tool memcheck ./a.out\n");
    return fails ? 1 : 0;
}
