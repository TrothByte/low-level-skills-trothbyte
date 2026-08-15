// GOOD: peer-to-peer copy with explicit bidirectional peer access and
// canAccessPeer verification, plus a p2p topology note.
//
// Target: nvcc -arch=sm_80 on a multi-GPU host (not available here).

#include <cstdio>
#include <cstdlib>

#define CHECK(fn) do { cudaError_t e_ = (fn); if (e_ != cudaSuccess) { \
    fprintf(stderr, "cuda: %s\n", cudaGetErrorString(e_)); exit(1); } } while (0)

int main(void)
{
    int dev0 = 0, dev1 = 1;
    cudaDeviceProp p0, p1;
    int can0to1 = 0, can1to0 = 0;

    CHECK(cudaGetDeviceProperties(&p0, dev0));
    CHECK(cudaGetDeviceProperties(&p1, dev1));
    CHECK(cudaDeviceCanAccessPeer(&can0to1, dev0, dev1));
    CHECK(cudaDeviceCanAccessPeer(&can1to0, dev1, dev0));

    if (!can0to1 || !can1to0) {
        fprintf(stderr, "peer access unavailable between %d and %d "
                "(check PCIe/NVLink topology, e.g. nvidia-smi topo -m); "
                "fall back to staged copies instead of assuming p2p\n", dev0, dev1);
        return 1;
    }

    // p2p is bidirectional: enable from both sides, then copy via UVA.
    CHECK(cudaSetDevice(dev0));
    CHECK(cudaDeviceEnablePeerAccess(dev1, 0));
    CHECK(cudaSetDevice(dev1));
    CHECK(cudaDeviceEnablePeerAccess(dev0, 0));

    const size_t bytes = 1 << 20;
    void *a = nullptr, *b = nullptr;
    CHECK(cudaSetDevice(dev0));
    CHECK(cudaMalloc(&a, bytes));
    CHECK(cudaSetDevice(dev1));
    CHECK(cudaMalloc(&b, bytes));

    CHECK(cudaSetDevice(dev0));
    CHECK(cudaMemcpyPeer(b, dev1, a, dev0, bytes));   // device-to-device
    CHECK(cudaDeviceSynchronize());

    // UVA check: on unified addressing both pointers are visible from either
    // device, which is what makes direct p2p copies legal.
    printf("p2p: %u bytes copied %d->%d (both directions enabled, "
           "UVA %d)\n", (unsigned)bytes, dev0, dev1, p0.unifiedAddressing && p1.unifiedAddressing);
    return 0;
}
