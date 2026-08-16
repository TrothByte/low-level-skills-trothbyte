# Interrupt Controllers — GIC & APIC Reference

Sources: `arm-arm` (exception model), `intel-sdm` Vol.3B (APIC), `cmsis`
(NVIC contrast), `kernel-source` (drivers/irqchip). GIC-specific normative
text is in the separate ARM GIC specification (proposed new source). The
Python routing model was executed on this host.

## 1. GIC interrupt ID ranges and routing

- **RULE**: in GICv3/4, SGIs are 0-15, PPIs 16-31 (per-core, e.g. timer),
  SPIs 32-1019 (shared, wired to the distributor). The distributor routes an
  SPI to a set of target CPU interfaces via `GICD_IROUTERn` (GICv3) /
  `GICD_ITARGETSRn` (GICv2); the redistributor owns PPIs/SGIs for a core.
- **WHY AI GETS IT WRONG**: "device IRQ 5" → programming 5 into the GIC
  instead of 32+5=37; mixing PPI and SPI ranges (B2/A10).
- **CORRECT REASONING**: Linux maps hardware IRQ → Linux IRQ with `irq_domain`
  and the driver must use the *controller's* ID; add 32 for SPI, use 16-31
  only for per-core peripherals.
- **EXAMPLE**: `examples/good/gic_routing_model.py` routes SPI 37 to CPU 0
  via the distributor model.
- **COUNTEREXAMPLE**: a driver registers SPI number 5 (actually an SGI) and
  the timer PPI gets masked instead.
- **VERIFICATION**: `python3 examples/good/gic_routing_model.py`; on target,
  `/proc/interrupts` shows the correct per-core distribution.
- **SOURCE**: ARM GIC spec (proposed); `arm-arm` (exception model).

## 2. EOI, priority drop, and the "lost interrupt"

- **RULE**: GIC requires an EOI to (a) drop priority and (b) deactivate the
  interrupt; on GICv2 both happen in one `GICC_EOIR` write, on GICv3 the
  active state is cleared via `ICC_EOIR1_EL1`/`ICC_DIR_EL1`. Missing the EOI
  leaves the IRQ blocked — the classic "interrupt fired once then never
  again" symptom.
- **WHY AI GETS IT WRONG**: assuming the CPU auto-EOIs; or doing an EOI on
  x86-style (a `mov` to the LAPIC) and leaving the GIC priority unmasked
  (B7).
- **CORRECT REASONING**: EOI is controller-specific and mandatory; on x86
  the LAPIC `EOI` register does the same job (`apic_eoi`). Linux `irq_chip`
  hides this; a raw driver must not.
- **EXAMPLE**: GICv3 handler: read `ICC_IAR1_EL1` (ack), service, then write
  `ICC_EOIR1_EL1` (EOI).
- **COUNTEREXAMPLE**: acknowledging but never EOIng — the IRQ stays
  priority-blocked.
- **VERIFICATION**: on target, flood the IRQ and diff `/proc/interrupts`;
  documented, not run here.
- **SOURCE**: ARM GIC spec (proposed); `arm-arm`.

## 3. x86 LAPIC/IO-APIC vector model

- **RULE**: each CPU has a local APIC (base at MSR `APIC_BASE`); IO-APIC
  redirection entries (RTEs) map an IRQ line to a vector (0-255, priority =
  vector/16); the LAPIC delivers to the CPU. The spurious vector is 0xFF.
- **WHY AI GETS IT WRONG**: thinking of IRQ numbers as global; forgetting
  that vector priority is by upper 4 bits; the 0xFF spurious vector storms
  when the LAPIC is not programmed.
- **CORRECT REASONING**: an IRQ = (IO-APIC RTE vector) → (LAPIC destination
  CPU) → interrupt gate. MSI devices write the vector into their own MSI
  address/data registers (translated by VT-d when remapping is on).
- **EXAMPLE**: `irq 16: IO-APIC 16-fasteoi` in `/proc/interrupts`.
- **COUNTEREXAMPLE**: programming the LAPIC but leaving the IO-APIC RTE
  unmasked — the IRQ fires forever (A10).
- **VERIFICATION**: `cat /proc/interrupts`, `dmesg | grep -i apic`; on
  target, not this host.
- **SOURCE**: `intel-sdm` Vol.3B §10 (APIC), §11 (IO-APIC).

## 4. Edge vs level and the stuck-IRQ storm

- **RULE**: level-triggered IRQs stay asserted until the device deasserts;
  edge-triggered IRQs are latched on a rising edge. A level IRQ with a
  device that never deasserts (and a driver that EOIs anyway) re-triggers
  immediately — the IRQ storm.
- **WHY AI GETS IT WRONG**: blaming the CPU or "too many interrupts" instead
  of the device deassert timing (B7).
- **CORRECT REASONING**: check the trigger register (`IRQF_TRIGGER_*` vs the
  GIC `GICD_ICFGRn` / IO-APIC RTE trigger bit) and the device's interrupt
  clear path; a shared level IRQ is especially easy to storm.
- **EXAMPLE**: `examples/bad/edge_level_confusion.py` simulates a level IRQ
  handled as edge → storm.
- **COUNTEREXAMPLE**: a correct level IRQ handler that first clears the
  device's pending bit, then EOIs.
- **VERIFICATION**: python simulation (host); target: `irq 16: ...` storm
  count.
- **SOURCE**: `intel-sdm` Vol.3B; `kernel-source` (kernel/irq).

## 5. Affinity and MSI remapping

- **RULE**: affinity steers delivery to specific CPUs: GIC via
  `GICD_IROUTERn`/redistributor, x86 via LAPIC destination + RTE dest field.
  MSI without remapping lets a device address physical memory directly —
  remapping (VT-d IR table / GIC ITS) confines MSI writes.
- **WHY AI GETS IT WRONG**: "affinity is cosmetic"; "MSI is always safe
  without an IOMMU" (A10).
- **CORRECT REASONING**: MSI writes to a physical address/data pair; with an
  IOMMU/ITS, the address is a handle the IOMMU translates — this is part of
  DMA isolation (see `iommu-smmu-isolation`).
- **EXAMPLE**: VFIO pass-through requires IRQ remapping to prevent a
  malicious device from forging MSI.
- **COUNTEREXAMPLE**: pass-through without VT-d/ITS — the device can write
  arbitrary vectors.
- **VERIFICATION**: `dmesg | grep -i "DMAR-IR"` / `its`; documented target.
- **SOURCE**: `intel-sdm` Vol.3B; `kernel-source` (drivers/iommu).
