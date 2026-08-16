# Evaluation — dma-cache-coherency

Skill: `skills/kernel/dma-cache-coherency`. Type: unique.
Stability: source-backed (API-contract stubs compiled and run with gcc 16.1
`-Werror` on this host); kernel DMA-API behavior on real hardware remains
INFERRED/UNVERIFIED (no Linux device host).

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| Correct direction/sync model | `examples/good/dma_direction_model.c` | prints 3 sync orders | compiles/runs |
| Missing sync on FROM_DEVICE | `examples/bad/dma_forgot_sync.c` | FLAG: no sync_for_cpu | compiles, silent |
| `dma_addr == 0` treated as error | `examples/bad/dma_forgot_sync.c` | FLAG: wrong error check | compiles, silent |
| Coherent vs streaming choice | descriptor ring vs bulk payload | ring→coherent, payload→streaming | reasoning eval |

## False-positive evals (correct code that must NOT be flagged)

- A coherent `dma_alloc_coherent` descriptor ring accessed by CPU and device
  concurrently — legal, must NOT be "fixed" to streaming.
- A `DMA_TO_DEVICE` buffer with `dma_sync_single_for_device` before the
  interrupt handler reads it — correct.
- A scatterlist fully mapped and fully unmapped with per-entry error unwind —
  correct.

## Historical evals

- **dma-debug / `CONFIG_DEBUG_DMA_API` class**: "DMA-API: device driver tries
  to free DMA memory it has not allocated" / leaked-mapping incidents —
  agent must recognize the API-ownership violation pattern.
- **ARM non-coherent DMA stale-data class**: DMA_FROM_DEVICE buffers read
  without sync on non-coherent SoCs (documented in the DMA-API docs as the
  canonical pitfall). Agent must explain why x86 hides it.
- **CVE-2018-15874-class (RNDIS/MSM)**: `skb` buffer reuse across DMA without
  `dma_sync_*` — flagged as stale/invalid data reuse.

## Adversarial evals (compiles-but-wrong)

- The bad fixture compiles cleanly with `-Wall -Wextra -Werror` and "works" on
  the x86 host — agent must flag it without a target build.
- A `dma_map_single` without `dma_mapping_error` where the map can fail
  (e.g., under swiotlb pressure).
- A driver that reads a coherent buffer with plain `volatile` reads and no
  barriers on a weakly-ordered CPU.

## Verification commands

Host (executed on this host):

```
gcc -Wall -Wextra -Werror -O2 examples/good/dma_direction_model.c -o /tmp/dma_good && /tmp/dma_good
gcc -Wall -Wextra -Werror -O2 examples/bad/dma_forgot_sync.c -o /tmp/dma_bad && /tmp/dma_bad
```

Target (documented, Linux host needed):

```
# kernel with CONFIG_DEBUG_DMA_API; run driver; check dmesg for API violations
dmesg | grep -i "DMA-API"
echo 10 > /sys/kernel/debug/dma-api/dump
```

## Verified facts (KNOWN / INFERRED / UNVERIFIED)

- KNOWN: both stubs compile with `-Werror` and run on this host; good prints
  the three sync orders and "mapping OK"; bad prints the no-sync readback.
- INFERRED: on a non-coherent ARM target the bad fixture returns stale data
  (researched from `arm-arm`/`kernel-driver-api`; no ARM hardware here).
- INFERRED: `CONFIG_DEBUG_DMA_API` flags the bad patterns (researched from
  `kernel-driver-api`).
- UNVERIFIED: actual DMA transfer behavior on real device hardware.

## Scoring

- Precision: high for API-contract violations (the stub model is executable
  and structurally verifiable). Recall: high for the mapped pattern classes;
  hardware-specific behavior is INFERRED. FP-rate: low — the sync discipline
  of correct code is unambiguous.

## Tooling availability (honest)

- Available on this host: gcc 16.1.0, python 3.11.9.
- NOT installed: Linux kernel build, actual device DMA hardware,
  `CONFIG_DEBUG_DMA_API` environment. Kernel-side verification commands are
  documented, not executed here.
