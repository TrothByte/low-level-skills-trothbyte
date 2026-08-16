# Evaluation — iommu-smmu-isolation

Skill: `skills/kernel/iommu-smmu-isolation`. Type: unique.
Stability: researched (Python SMMU translation model executed on this host;
ARM SMMU/Intel VT-d hardware absent — hardware behavior INFERRED/UNVERIFIED).

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| StreamID lookup + translation | `examples/good/smmu_translation_model.py` | 0x8000, fault on unmapped | runs (see facts) |
| Device-TLB invalidation after map | `examples/good/smmu_translation_model.py` | tlbi_count=1 | runs |
| Passthrough bypass hole | `examples/bad/passthrough_bypass.py` | FLAG: no isolation | runs |
| SMMU StreamID vs BDF | reasoning case | StreamID for SMMU | eval case |

## False-positive evals (correct code that must NOT be flagged)

- A properly attached device with an `iommu_domain` that maps only its
  window — correct.
- A documented, justified passthrough for a trusted on-board NIC in a
  single-tenant system — legal (with the documentation).
- `iommu_map` + implicit IOMMU-API TLBI — correct; must NOT be flagged as
  missing invalidation.
- A stage-2 mapping for a VFIO guest device — correct.

## Historical evals

- **DMAR DMA-fault class** — `DMAR: [DMA Read] request device [02:00.0]
  fault addr ...` logs; agent must map the BDF/IOVA to the missing PTE and
  fix the mapping, not the device.
- **VT-d/AMD-Vi bypass bugs in older kernels** — devices with bypass/passthrough
  configurations that let guest DMA reach host memory (documented in kernel
  iommu commits); agent must recognize the isolation hole.
- **SMMU fault storms after PTE update without TLBI** — the device keeps a
  stale translation; agent must require device-TLB invalidation, not a CPU
  `sfence.vma`.

## Adversarial evals (compiles-but-wrong)

- The bad fixture runs and shows writes to arbitrary physical addresses — the
  model is demonstrably unsafe.
- Code that calls `sfence.vma` after a device-table update and claims the
  device TLB is flushed — must be rejected (device TLB is separate).
- A guest-attached device in passthrough with no stage-2 — must be flagged as
  a host-memory hole.

## Verification commands

Host (executed on this host):

```
python3 examples/good/smmu_translation_model.py
python3 examples/bad/passthrough_bypass.py
```

Target (documented, toolchain not on this host):

```
ls /sys/kernel/iommu_groups/
dmesg | grep -i "DMAR|iommu|smmu"
qemu-system-x86_64 -machine q35 -device intel-iommu ...
```

## Verified facts (KNOWN / INFERRED / UNVERIFIED)

- KNOWN: `smmu_translation_model.py` runs on this host and prints the
  translated address, the fault for an unmapped IOVA, and tlbi_count=1.
- KNOWN: `passthrough_bypass.py` runs and shows unbounded physical access —
  the isolation hole is demonstrated.
- INFERRED: SMMU uses StreamID and requires CMDQ TLBI commands; VT-d uses BDF
  and IOTLB invalidation (researched from `kernel-driver-api` + SMMU/VT-d
  specs).
- UNVERIFIED: real SMMU/VT-d behavior on actual hardware or QEMU with vIOMMU.

## Scoring

- Precision: high for the modeled isolation semantics (executable).
- Recall: high for the documented classes; hardware specifics are INFERRED.
- FP-rate: low — correct domain/attach/TLBI patterns are distinguishable from
  bypass.

## Tooling availability (honest)

- Available on this host: python 3.11.9 (both fixtures run).
- NOT installed: ARM SMMU hardware, Intel VT-d platform, QEMU vIOMMU
  configuration. Target commands documented, not executed here.
