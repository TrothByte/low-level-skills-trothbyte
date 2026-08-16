# DMA & Cache Coherency — Reference

Sources: `kernel-driver-api` (DMA-API), `ldd3` ch.15, `linux-memory-barriers`,
`intel-sdm` Vol.3A cache chapter. Linux API claims are researched from the
kernel docs; the direction model and error-check flow below are exercised by
the host-compilable stubs.

## 1. Coherent vs streaming mappings

- **RULE**: coherent mappings (`dma_alloc_coherent`) keep CPU and device views
  consistent for concurrent access (on ARM often uncached or via coherent
  domains); streaming mappings (`dma_map_single`) transfer ownership to the
  device and require explicit `dma_sync_*`/unmap before the CPU reads.
- **WHY AI GETS IT WRONG**: "DMA = dma_alloc_coherent", or "the API makes my
  kmalloc buffer uncached" — neither is true in general (B2).
- **CORRECT REASONING**: streaming maps an existing cacheable buffer for one
  transfer; coherent allocates a dedicated region for long-lived shared
  structures (rings, descriptors). Coherent is not free: it pins memory and
  often uncaches it, hurting CPU access.
- **EXAMPLE**: NIC RX/TX rings are coherent; the payload buffers they point to
  are streamed (`dma_map_single` per packet).
- **COUNTEREXAMPLE**: `dma_alloc_coherent` for every 4 KiB packet buffer.
- **VERIFICATION**: host stub — coherent vs streaming struct shows ownership
  direction; target: `CONFIG_DEBUG_DMA_API` run of the driver.
- **SOURCE**: `kernel-driver-api` (DMA-API: coherent vs streaming);
  `ldd3` ch.15.

## 2. dma_map_* direction and sync discipline

- **RULE**: `DMA_TO_DEVICE` = CPU writes then device reads (flush caches
  before giving to device); `DMA_FROM_DEVICE` = device writes then CPU reads
  (invalidate after); `DMA_BIDIRECTIONAL` = both. The CPU must not touch the
  buffer while mapped; to reuse, call `dma_sync_single_for_cpu` then later
  `dma_sync_single_for_device`.
- **WHY AI GETS IT WRONG**: reading a `DMA_FROM_DEVICE` buffer right after
  the interrupt without sync — on non-coherent ARM this reads stale cached
  lines (A10); choosing TO_DEVICE for a device-write transfer corrupts data.
- **CORRECT REASONING**: model ownership: after `dma_map_single` the device
  owns the buffer; the CPU takes it back with `dma_unmap_*` or
  `dma_sync_single_for_cpu`. On x86 (coherent platform) the sync is often a
  no-op, which is why the bug hides on the dev machine.
- **EXAMPLE**: `dma_sync_single_for_cpu(dev, dma_handle, size, DMA_FROM_DEVICE)`
  before reading the received data.
- **COUNTEREXAMPLE**: reading the same buffer without any sync after the
  device finished — stale data on non-coherent systems.
- **VERIFICATION**: host stub `examples/good/dma_direction_model.c` prints
  the required sync order for each direction; `examples/bad/dma_forgot_sync.c`
  omits it (structurally incorrect).
- **SOURCE**: `kernel-driver-api` (streaming DMA mapping, sync);
  `linux-memory-barriers` (device memory ordering).

## 3. Cache-line granularity and bounce buffers

- **RULE**: non-coherent DMA requires transfers aligned to the cache line;
  partial-line transfers risk tearing, and the DMA layer uses bounce buffers
  when the device or address constraints require it.
- **WHY AI GETS IT WRONG**: assuming byte-granular DMA is safe on any system.
- **CORRECT REASONING**: cache maintenance works on lines (typically 32-64
  bytes); a transfer that ends mid-line leaves the rest of the line
  inconsistent between CPU and device. The kernel's swiotlb/bounce path hides
  this at a cost; drivers should align when they can.
- **EXAMPLE**: a 512-byte aligned network RX buffer — no bounce needed.
- **COUNTEREXAMPLE**: a 3-byte offset DMA to a 64-byte cache line on a
  non-coherent SoC — torn data.
- **VERIFICATION**: host stub checks alignment vs a configurable line size;
  target: `dmesg` "n_hugepages"/swiotlb activity.
- **SOURCE**: `kernel-driver-api`; `intel-sdm` Vol.3A (cache line size).

## 4. DMA-API error handling

- **RULE**: check `dma_mapping_error()` after every `dma_map_*`; on failure,
  unmap all previously-mapped entries of the current scatterlist. Never treat
  a `dma_addr_t` of 0 as failure — it is valid on many platforms.
- **WHY AI GETS IT WRONG**: `if (!dma_addr) { error }` is an x86-ism that
  misclassifies valid zero addresses on other architectures.
- **CORRECT REASONING**: use the API's own error predicate, and unwind partial
  maps.
- **EXAMPLE**: `if (dma_mapping_error(dev, addr)) { goto err_unmap; }`.
- **COUNTEREXAMPLE**: `if (addr == 0) { goto err; }`.
- **VERIFICATION**: host stub compiles and the bad variant is structurally
  flagged; target: `CONFIG_DEBUG_DMA_API` reports "not checked" leaks.
- **SOURCE**: `kernel-driver-api` (DMA-API: error handling).
