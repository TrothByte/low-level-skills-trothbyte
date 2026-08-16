// GOOD: DMA direction and ownership model, host-compilable standalone model
// of the Linux DMA-API contract. Every map is checked with dma_mapping_error,
// and sync order follows the direction.
// Compile: gcc -Wall -Wextra -Werror -O2 dma_direction_model.c -o /tmp/dma_good && /tmp/dma_good
#include <stdio.h>

typedef unsigned long dma_addr_t;
typedef enum { DMA_TO_DEVICE = 1, DMA_FROM_DEVICE = 2, DMA_BIDIRECTIONAL = 3 } dma_direction_t;

#define DMA_MAPPING_ERROR ((dma_addr_t)-1)

// GOOD: always check map results with the API predicate, never "addr == 0".
static int dma_mapping_error(dma_addr_t addr) { return addr == DMA_MAPPING_ERROR; }

// GOOD: model the required sync discipline for each direction.
static const char *sync_order(dma_direction_t dir) {
    switch (dir) {
    case DMA_TO_DEVICE:     return "CPU:write -> sync_for_device -> DEV:read";
    case DMA_FROM_DEVICE:   return "DEV:write -> sync_for_cpu -> CPU:read";
    case DMA_BIDIRECTIONAL: return "sync_for_device; DEV:I/O; sync_for_cpu";
    }
    return "?";
}

int main(void) {
    dma_addr_t a = 0x1000;   // GOOD: a 0 dma_addr_t is a VALID address
    if (dma_mapping_error(a)) {
        printf("map failed\n");
        return 1;
    }
    printf("DMA_TO_DEVICE: %s\n", sync_order(DMA_TO_DEVICE));
    printf("DMA_FROM_DEVICE: %s\n", sync_order(DMA_FROM_DEVICE));
    printf("mapping OK (addr=%#lx)\n", (unsigned long)a);
    return 0;
}
