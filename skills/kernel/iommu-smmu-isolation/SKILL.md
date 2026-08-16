---
name: iommu-smmu-isolation
description: Use when writing or reviewing IOMMU/SMMU code — DMA isolation, stream/context tables, stage-1/2 translation, iommu_domain, VT-d vs SMMU, passthrough vs translation, device TLB invalidation, and DMA faults. Teaches IOMMU architecture and how to keep devices from bypassing isolation.
---

# IOMMU / SMMU DMA Isolation

## When to use

- Enabling DMA isolation for a device (`iommu_domain`, `iommu_attach_device`).
- Writing or reviewing an SMMU (ARM) or VT-d (Intel) driver/configuration.
- VFIO/device passthrough in hypervisors (stage-2 DMA translation).
- Auditing whether a device can DMA anywhere (bypass/passthrough hole).
- Debugging DMA faults (translation errors, `DMAR: [DMA Write]` logs).
- Setting up interrupt remapping (IR table / ITS) for MSI.

## When not to use

- CPU page tables (that's `page-table-management`).
- Devices with no DMA capability.
- Pure cache-coherency work without translation (that's `dma-cache-coherency`).
- Userspace buffer management.

## What the agent often gets wrong

- "The IOMMU is just the CPU's MMU for devices" — it is a separate walk with
  its own tables (stream/context), its own TLB, and its own invalidation
  commands; `INVLPG`/`sfence.vma` do NOT flush device TLB entries (B2).
- "Passthrough mode is fine because it's how my device was configured" —
  passthrough/bypass means the device's DMA goes straight to physical memory:
  a security hole when the device is untrusted (A10).
- Confusing SMMU StreamID with the device's PCI BDF — the SMMU uses the
  StreamID from the SoC interconnect; VT-d uses BDF. Mixing them breaks the
  lookup.
- Forgetting stage-2 (guest) translation: passthrough a device without
  stage-2 mapping lets the guest DMA into host memory.
- Fault handling: a translation fault aborts the device transaction; ignoring
  fault records hides the cause ("my DMA just hangs").
- Assuming the IOMMU makes DMA coherent — it translates addresses, it does
  not flush/invalidate CPU caches (that's the DMA-API's job).
- PTE changes without device-TLB invalidation (SMMU `TLBI`, VT-d IOTLB
  invalidate) — the device keeps the stale translation.

## How to reason correctly

1. Identify the IOMMU and its identity mapping: SMMUv3 — StreamID → stream
   table entry → context descriptor → (stage 1 and/or stage 2) page tables;
   VT-d — BDF → root/context table → page tables; AMD-Vi — device table.
2. Decide translation vs passthrough per device: translate untrusted devices;
   keep passthrough only for trusted devices whose DMA range is inherently
   bounded (and document the exception).
3. Create an `iommu_domain` (or SMMU stream), attach the device, and map the
   device-visible addresses with `iommu_map`/`iommu_dma_*`.
4. For guests/VFIO: provide stage-2 translation so the guest's device DMA
   lands in guest memory only; never attach a passthrough device to a guest
   without the guest MMU/IOMMU protecting host memory.
5. After any device page-table change, issue the device-TLB invalidation
   (SMMU CMDQ `TLBI`; VT-d `IOTLB` + `Device-TLB` invalidation).
6. Handle faults: read the fault record (SMMU `EVTQ`, VT-d `DMAR` logs) and
   respond (abort the transaction, log, recover).
7. Verify: with the IOMMU enabled, an un-mapped DMA address faults; with
   passthrough it does not — use that contrast as the isolation test.

## What to verify

- Every DMA-capable device has a stream/context (or domain) mapping — no
  accidental bypass.
- Device page-table changes are followed by device-TLB invalidation, not just
  CPU `sfence.vma`/`invlpg`.
- Passthrough is documented/justified for each device that uses it.
- Guest devices use stage-2 (or a nested IOMMU) so host memory is isolated.
- Fault handling is present and logs the StreamID/BDF of the offender.
- Interrupt remapping (IR table/ITS) is enabled for MSI devices.

## How to verify

Host-executable simulation (Python, self-contained):

```
python3 examples/good/smmu_translation_model.py  # correct stream walk + TLBI
python3 examples/bad/passthrough_bypass.py       # bypass hole simulation
```

Target checks (documented, toolchain not on this host):

```
# Linux: inspect IOMMU groups and DMA windows
ls /sys/kernel/iommu_groups/
dmesg | grep -i "DMAR|iommu|smmu"   # faults and remapping state
# QEMU with vIOMMU: boot with -device intel-iommu / -machine virt with SMMU
qemu-system-x86_64 -machine q35 -device intel-iommu ...
```

## Where the knowledge comes from

- `kernel-driver-api` — IOMMU API (domain, map/unmap, attach)
- `kernel-source` — drivers/iommu (SMMUv3, VT-d, AMD-Vi), include/linux/iommu.h
- `intel-sdm` — VT-d is a separate spec (proposed new source, see report)
- ARM SMMU architecture spec (proposed new source, see report)
- `cwe` — CWE-284/269 (improper access control via bypass) (recommend)

## Related skills

- `dma-cache-coherency` — the DMA-API layer the IOMMU sits under (recommend)
- `page-table-management` — CPU MMU concepts that inform device tables (recommend)
- `kernel-uaccess-safety` — kernel/user boundaries in the same trust model (recommend)
- `kernel-driver-char-device-lifecycle` — where IOMMU attach lives in a driver (recommend)
- `hypervisor-vmx-svm-internals` — stage-2/nested translation for guests (recommend)

## Evaluation

Synthetic: classify SMMU StreamID vs BDF; flag a missing device-TLB flush
after `iommu_map`; approve a proper `iommu_domain` + stage-2 for VFIO.
Adversarial: a passthrough device attached to a guest with no stage-2 — must
be flagged as a host-memory hole; "the IOMMU flushes my CPU TLB" — must be
rejected. Historical: DMAR DMA-fault log classes (e.g., "DMAR: [DMA Read]
request device [02:00.0]"), VT-d/AMD-Vi bypass bugs in older kernels, and
SMMU fault storms after a PTE update without TLBI. FP: a correctly translated
device with a proper domain and a documented, justified passthrough for a
trusted NIC must NOT be flagged.
