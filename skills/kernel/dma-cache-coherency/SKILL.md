---
name: dma-cache-coherency
description: Use when writing or reviewing DMA code — dma_alloc_coherent vs streaming mappings, dma_map_single/sg, dma_sync_single_for_cpu/device, DMA direction, cache-coherent vs non-coherent devices, bounce buffers, and stale-data bugs. Teaches the Linux DMA-API contract and the cache-coherency rules that keep device and CPU views consistent.
---

# DMA & Cache Coherency

## When to use

- Allocating DMA buffers (`dma_alloc_coherent`, `dma_pool`).
- Mapping/unmapping buffers for streaming DMA (`dma_map_single`, `dma_map_sg`,
  `dma_unmap_*`, `dma_sync_single_for_cpu/device`).
- Choosing between coherent and streaming mappings for a driver.
- Debugging stale/tearing data after a DMA transfer (cache lines not flushed
  or invalidated).
- Drivers that DMA from/to network, storage, audio, or camera devices.
- Reasoning about bounce buffers and IOMMU interaction.

## When not to use

- MMIO/register access with `readl/writel` and `pgprot_noncached` (that's
  `embedded-volatile-and-memory-ordering` + MMIO rules).
- Pure CPU-side cache optimization (`cache-and-numa-optimization`).
- Userspace buffer sharing without a kernel driver in the path.
- If the device performs no DMA at all (interrupt-only devices).

## What the agent often gets wrong

- "`dma_alloc_coherent` is the only correct way to do DMA" — it is
  expensive/pinned and often the *wrong* choice for streaming transfers where
  the buffer lives in a normal cacheable page; coherent vs streaming is an
  ownership question, not a "DMA is DMA" one (B2).
- "On x86 everything is cache-coherent so I can skip the API" — the API is
  still required; skipping `dma_map_single` is a correctness bug even when the
  hardware happens to be coherent (A10).
- "`dma_map_single` makes the buffer uncacheable" — it transfers ownership to
  the device; the CPU must call `dma_sync_single_for_cpu` (or the implicit
  `dma_unmap` sync) before reading, and `dma_sync_single_for_device` before
  the device reads CPU-written data.
- Forgetting the direction: `DMA_TO_DEVICE` (flush before), `DMA_FROM_DEVICE`
  (invalidate after), `DMA_BIDIRECTIONAL` (both). Using the wrong direction
  silently corrupts data.
- `dma_map_*` return values: not checking `dma_mapping_error()` after a map,
  and using a dma_addr_t of 0 as "valid" — 0 is a valid DMA address on many
  architectures.
- Unmapping in the wrong order or not at all (leak), and unmapping a partial
  scatterlist after a mid-list map failure.
- Cache-line granularity: DMA transfers must be aligned/sized to the cache
  line or you get partial-line tearing on non-coherent systems; bounce buffers
  exist precisely for this.

## How to reason correctly

1. Decide the mapping class. Streaming (`dma_map_*`): the CPU does not touch
   the buffer while it is mapped to the device; ownership alternates. Coherent
   (`dma_alloc_coherent`): CPU and device may access concurrently; the
   architecture hides cache effects (uncached/coherent mapping). Choose
   streaming for bulk transfers, coherent for descriptors/rings touched by both.
2. Map with the correct direction, check `dma_mapping_error`.
3. When the CPU must read device-written data, synchronize: either call
   `dma_sync_single_for_cpu` (then later `dma_sync_single_for_device` if
   reusing) or unmap — never read a mapped-for-device buffer directly.
4. Respect alignment/size: keep transfers within cache-line boundaries where
   the device is non-coherent; let the DMA layer bounce otherwise.
5. Unmap exactly once, in map order, and free coherent memory with
   `dma_free_coherent` (matching allocator, not `kfree`).
6. On non-coherent architectures the API inserts the necessary cache
   maintenance; never hand-roll `dma_cache_wback_inv` unless the DMA-API docs
   explicitly require it.

## What to verify

- Every `dma_map_*` is paired with `dma_mapping_error()` and a matching
  `dma_unmap_*` in all error paths.
- Direction is correct for the transfer.
- CPU reads/writes of the buffer are flanked by the correct sync calls (or
  the buffer is coherent).
- Coherent memory is freed with the matching API.
- No DMA into the kernel stack, read-only data, or `kmalloc` buffers that were
  never mapped.
- Scatter-gather lists are fully mapped and fully unmapped.

## How to verify

Host-compilable logic check (self-contained, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/good/dma_direction_model.c -o /tmp/dma_good
gcc -Wall -Wextra -Werror -O2 examples/bad/dma_forgot_sync.c -o /tmp/dma_bad
```

Documented kernel/target checks (kernel tree + Linux host needed, not run here):

```
# CONFIG_DEBUG_DMA_API + dma-debug catches leaks and API misuse
echo 10 > /sys/kernel/debug/dma-api/dump   # dump outstanding mappings
# dmesg: "DMA-API: device driver ... does not check map errors" etc.
```

## Where the knowledge comes from

- `kernel-driver-api` — DMA-API documentation (coherent vs streaming, sync)
- `ldd3` — Chapter 15 (DMA), still the canonical API walkthrough
- `linux-memory-barriers` — device memory ordering and DMA-cache rules
- `intel-sdm` — Vol.3A cache chapter (cache line granularity)
- `arm-arm` — cache coherency model (non-coherent DMA maintenance)
- `kernel-source` — drivers/iommu, arch/*/mm dma_map_* implementation

## Related skills

- `kernel-driver-char-device-lifecycle` — where DMA lives in a driver (recommend)
- `kernel-uaccess-safety` — buffers crossing user/kernel also need ownership rules (recommend)
- `embedded-volatile-and-memory-ordering` — MMIO vs DMA memory handling (recommend)
- `cache-and-numa-optimization` — CPU-side cache behavior (recommend)
- `kernel-atomic-context` — what DMA/sync calls are legal in atomic context (recommend)

## Evaluation

Synthetic: choose coherent vs streaming for a descriptor ring (coherent) vs a
bulk transfer (streaming); flag a missing `dma_mapping_error`; flag a
`DMA_FROM_DEVICE` buffer read without sync. Adversarial: code that "works on
x86" because of hardware coherency but is broken on ARM — must be flagged
without a target build; a `dma_alloc_coherent` used for a hot streaming path
(must suggest streaming). Historical: `dma-debug` "leaked DMA mapping"
incidents and the classic ARM non-coherent DMA stale-data class (CVE-2018-...;
documented in the DMA-API docs as "DMA-FROM-DEVICE and the sync"). FP: a
correct coherent buffer with concurrent CPU/device access must NOT be flagged.
