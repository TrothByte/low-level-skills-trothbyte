# IOMMU / SMMU Isolation — Reference

Sources: `kernel-driver-api` (IOMMU API), `kernel-source` (drivers/iommu).
Normative details for ARM SMMUv3 and Intel VT-d are in their separate
specifications (proposed new sources). The Python translation model was
executed on this host.

## 1. SMMU StreamID vs VT-d BDF identity

- **RULE**: ARM SMMU (v2/v3) identifies a device by a StreamID derived from
  the SoC interconnect (often a flattened bus/function); Intel VT-d identifies
  it by PCI BDF (bus:device.function). The stream/context lookup must use the
  right identity — a BDF is not a StreamID.
- **WHY AI GETS IT WRONG**: applying the PCI BDF to an SMMU and vice versa;
  the wrong entry is selected and translation silently misconfigures (B2).
- **CORRECT REASONING**: read the SoC's StreamID assignment (device tree /
  ACPI for SMMU) and the PCI BDF from the configuration space; SMMUv3 stream
  table is indexed by StreamID, VT-d context table by BDF.
- **EXAMPLE**: `examples/good/smmu_translation_model.py` indexes the stream
  table by a StreamID, not by a BDF.
- **COUNTEREXAMPLE**: attaching a device to the SMMU using its PCI BDF as if
  it were the StreamID — the stream table lookup returns a garbage entry.
- **VERIFICATION**: python model (host); target: `dmesg` SMMU stream-table
  faults for the specific StreamID.
- **SOURCE**: ARM SMMU spec (proposed); `kernel-source` (drivers/iommu).

## 2. Device TLB invalidation is not CPU TLB invalidation

- **RULE**: the IOMMU has its own TLB (SMMU: per-stream or global via the
  command queue; VT-d: IOTLB + per-device TLBs). After a PTE change in a
  device page table you must issue an IOMMU TLB invalidation command —
  `invlpg`/`sfence.vma`/`tlbi` on the CPU do nothing for devices.
- **WHY AI GETS IT WRONG**: treating the IOMMU as "the MMU, for devices"
  (B2/A10) and reusing CPU TLB reasoning.
- **CORRECT REASONING**: SMMUv3 sends `CMDQ_TLBI_*` commands; VT-d writes
  IOTLB/device-TLB invalidation descriptors to the invalidation queue. The
  Linux IOMMU API issues these on `iommu_unmap`/`iommu_map` — hand-rolled
  table writes must too.
- **EXAMPLE**: after `iommu_map(d, iova, pa, size)`, the SMMU command queue
  gets a `TLBI_EL2`/`TLBI_NH_VA`; the device sees the new mapping.
- **COUNTEREXAMPLE**: writing the SMMU stream-table entry and only executing
  `sfence.vma` on the CPU — the device keeps the stale translation.
- **VERIFICATION**: python model prints the TLBI issued after a map;
  target: DMA fault log if missing.
- **SOURCE**: `kernel-driver-api` (IOMMU API); ARM SMMU spec (proposed).

## 3. Passthrough vs translation — the isolation decision

- **RULE**: passthrough (bypass) mode maps the device's DMA to physical
  memory 1:1; translation mode confines DMA to the IOVA range mapped in the
  domain. Passthrough is a security hole when the device is untrusted or
  when a guest owns the device.
- **WHY AI GETS IT WRONG**: "it works, leave it" — bypass silently disables
  isolation (A10).
- **CORRECT REASONING**: default to translation; use passthrough only for
  trusted devices with a documented, bounded DMA range (or when the platform
  requires it for correctness), and never for a device attached to a VM
  without an additional isolation layer.
- **EXAMPLE**: `examples/bad/passthrough_bypass.py` shows a bypass that
  writes to any physical address.
- **COUNTEREXAMPLE**: a correct translation domain that faults on an
  un-mapped address — the isolation test.
- **VERIFICATION**: python model (host); target: `iommu=pt` vs default,
  `dmesg` IOVA faults.
- **SOURCE**: `kernel-driver-api`; `cwe` CWE-284 (improper access control).

## 4. Stage-1/stage-2 and guest isolation

- **RULE**: SMMUv3 and VT-d support two-stage translation: stage-1 is the
  device/guest OS view, stage-2 is the hypervisor's. Attaching a device to a
  guest requires a stage-2 that maps only guest memory — otherwise the guest
  device can DMA into host memory.
- **WHY AI GETS IT WRONG**: "the VM has its own page tables, so DMA is
  fine" — the guest's MMU does not apply to device DMA (B7).
- **CORRECT REASONING**: VFIO creates a container/domain with a stage-2
  mapping of the guest's DMA window; `iommu_attach_device` to the container's
  domain. Nested translation (stage-1 by guest + stage-2 by VMM) is the
  secure default.
- **EXAMPLE**: VFIO + intel-iommu: guest IOVA → stage-2 → host memory.
- **COUNTEREXAMPLE**: passthrough device in a guest without stage-2 — the
  guest's driver can read/write arbitrary host physical memory.
- **VERIFICATION**: target: VFIO test device, `dmesg` IOVA fault on a bogus
  guest IOVA; documented.
- **SOURCE**: `kernel-driver-api` (VFIO/IOMMU); VT-d spec (proposed).

## 5. DMA faults and fault records

- **RULE**: a translation fault aborts the device transaction and is recorded
  in the IOMMU fault log (SMMUv3 event queue, VT-d DMAR fault records). The
  log contains the StreamID/BDF, IOVA, and read/write bit — the first stop
  when DMA "hangs" or a device misbehaves.
- **WHY AI GETS IT WRONG**: debugging "DMA hangs" by checking the device
  only; the fault log names the exact cause.
- **CORRECT REASONING**: read `dmesg` for `DMAR:`/SMMU event entries; match
  the BDF/StreamID against the device tree; fix the mapping, not the device.
- **EXAMPLE**: `DMAR: [DMA Write] Request device [02:00.0] fault addr
  0x...` — a PTE was missing for that IOVA.
- **COUNTEREXAMPLE**: a driver with `iommu_map` that silently failed
  (returned error) and the device DMA now faults.
- **VERIFICATION**: python model faults on an unmapped IOVA; target: kernel
  log check.
- **SOURCE**: `kernel-source` (drivers/iommu); ARM SMMU spec (proposed).
