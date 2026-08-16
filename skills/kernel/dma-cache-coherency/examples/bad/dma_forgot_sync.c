// BAD: DMA_FROM_DEVICE buffer read without any sync, and a map-result check
// that treats dma_addr_t == 0 as failure. Structurally incorrect against the
// DMA-API contract; on x86 (coherent) it happens to work, on non-coherent
// ARM it reads stale cache lines.
// intentionally incorrect
#include <stdio.h>

typedef unsigned long dma_addr_t;
typedef enum { DMA_FROM_DEVICE = 2 } dma_direction_t;

#define DMA_MAPPING_ERROR ((dma_addr_t)-1)

static dma_addr_t map_buffer(void) { return 0x1000; }

static unsigned char buffer[64];

// BAD: missing dma_sync_single_for_cpu before the CPU read.
static unsigned char read_back(void) {
    return buffer[0];
}

int main(void) {
    dma_addr_t dma = map_buffer();
    if (dma == 0) {              // BAD: 0 is a valid DMA address on many archs
        printf("map failed (wrong check)\n");
        return 1;
    }
    buffer[0] = read_back();     // BAD: no sync, stale data on non-coherent
    (void)dma; (void)DMA_MAPPING_ERROR; (void)DMA_FROM_DEVICE;
    printf("read back byte=%u (no sync performed)\n", buffer[0]);
    return 0;
}
