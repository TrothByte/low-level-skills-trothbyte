---
name: interrupt-controller-gic-apic
description: Use when writing or reviewing interrupt handling — ARM GICv3/v4 routing (SPI/PPI/SGI, distributor, redistributor), x86 APIC (LAPIC, IO-APIC, MSI, spurious vectors), priority/nesting, EOI, edge vs level, affinity, and IRQ remapping. Teaches the two controller families' structure and the driver contract for correct IRQ delivery.
---

# Interrupt Controllers: ARM GIC & x86 APIC

## When to use

- Writing or reviewing IRQ handlers, `request_irq`/threaded IRQ setup, or
  irqchip drivers.
- Configuring trigger types (edge/level), priorities, masking, and EOI.
- Debugging lost, stuck, or spurious interrupts (IRQ storms, missed IRQ).
- Setting IRQ affinity/balancing (`irq_set_affinity`, CPU maps).
- MSI/MSI-X setup and interrupt remapping.
- Porting interrupt code between ARM (GIC) and x86 (APIC).

## When not to use

- Cortex-M NVIC-specific code (see `cmsis` / `embedded-interrupt-and-nested`).
- Software interrupts/timers inside the scheduler (kernel-scheduler domain).
- Userspace signal handling.
- Low-level ISR latency tuning without controller interaction (perf domain).

## What the agent often gets wrong

- Treating GIC and APIC as "the same thing" — GIC is a distributor-based
  interrupt-ID model; x86 APIC is vector-based with LAPIC+IO-APIC+MSI (B2).
- GIC interrupt numbers: SPI = 32..1019, PPI = 16..31, SGI = 0..15 — off-by
  one/N+32 confusion is the classic GIC bug (A10).
- GICv3 requires an EOI (priority drop + deactivate); skipping it leaves the
  interrupt masked — the "lost interrupt" symptom.
- x86: forgetting the spurious vector (0xFF) and that the LAPIC is at
  MSR `APIC_BASE`; an unmasked, unhandled IRQ vector 0xFF fires forever.
- Edge vs level semantics: level-triggered IRQs need the device to
  deassert before EOI or they re-trigger (stuck IRQ storm).
- `irq_disable`/`enable` vs masking inside the controller: a handler that
  re-enables interrupts before the controller is done causes re-entrancy.
- MSI without interrupt remapping support (VT-d/ITS) — device writes a raw
  vector/address; with remapping, the MSI address is translated via the IR
  table (IOMMU skill).
- Nesting: priority drop (GIC) vs `IRQF_NESTED` vs threaded IRQ — using the
  wrong model deadlocks or re-enters the same handler (B7).

## How to reason correctly

1. Identify the controller: ARM GICv3/4 (distributor + redistributor +
   CPU interface, interrupt-ID based, security groups 0/1), x86 (local APIC
   per CPU + IO-APIC(s) + MSI/MSI-X + interrupt remapping via VT-d).
2. Map the source: GIC — SGI/PPI/SPI per core/distributor; x86 — IRQ line →
   IO-APIC RTE → LAPIC vector (priority = vector/16).
3. Register the handler with the right flags: `IRQF_TRIGGER_*`, shared IRQ,
   `IRQF_NO_THREAD`, threaded IRQ for sleeping handlers; never sleep in a
   hard IRQ context.
4. Implement EOI/masking correctly: GICv3 `write_sysreg` EOIR/ICC_DIR,
   or the `irq_chip` `irq_eoi`/`irq_ack`; x86: `apic_eoi`/`irq_ack` after the
   handler.
5. Configure affinity explicitly when the device or latency requires it;
   respect the kernel's IRQ migration (`irq_set_affinity_hint`).
6. On a stuck/lost IRQ: check masking, trigger type, EOI, and the device's
   pending state — in that order.
7. Verify with `/proc/interrupts` (x86) or GIC register dumps (ARM), and a
   stress test (flood the IRQ, count handled interrupts).

## What to verify

- IRQ numbers map to the correct SPI/PPI range (GIC) or IO-APIC RTE (x86).
- EOI/ack is emitted exactly once per interrupt.
- Trigger type matches the device (edge vs level).
- No sleeping functions in hard IRQ context.
- Affinity set for the expected CPU(s); no lost migration.
- MSI path has interrupt remapping enabled or documented as unsafe.
- No re-entrancy of the same handler.

## How to verify

Host-executable simulation (Python, self-contained):

```
python3 examples/good/gic_routing_model.py   # correct SPI→CPU routing
python3 examples/bad/edge_level_confusion.py # missing deassert → storm sim
```

Target checks (documented, toolchain not on this host):

```
# ARM GIC: boot Linux on QEMU virt machine, inspect /proc/interrupts + GIC regs
qemu-system-aarch64 -machine virt -cpu cortex-a57 -kernel <kernel> -nographic
# x86: irqbalance status, /proc/interrupts diff during a flood test
cat /proc/interrupts
```

## Where the knowledge comes from

- `arm-arm` — exception/interrupt model (AArch64); GIC spec is separate
  (proposed new source, see report)
- `intel-sdm` — Vol.3B APIC chapter (local APIC, IO-APIC, vector model)
- `cmsis` — NVIC contrast (Cortex-M vs GIC)
- `kernel-source` — drivers/irqchip (GIC/APIC drivers), kernel/irq
- `freertos-docs` — ISR-to-task semantics for RTOS side (compare)

## Related skills

- `embedded-interrupt-and-nested` — NVIC/nesting rules (recommend)
- `kernel-atomic-context` — what is legal in IRQ context (require)
- `rtos-concurrency-and-isr` — ISR-to-task handoff (recommend)
- `kernel-driver-char-device-lifecycle` — IRQ ownership in drivers (recommend)
- `hypervisor-vmx-svm-internals` — virtualized interrupts (APICv/ITS) (recommend)

## Evaluation

Synthetic: map a device IRQ to the correct GIC SPI number (add 32); classify
x86 IRQ→vector→priority; flag a missing GIC EOI; flag edge/level mismatch.
Adversarial: a handler that "works on x86" but uses x86 EOI semantics on GIC
— must be flagged; a spurious IRQ storm diagnosed as "device broken" when the
real cause is missing EOI. Historical: real GIC and IO-APIC integration bugs
(document in Linux kernel git; e.g., GIC IRQ storm after `irq_disable` misuse,
x86 spurious vector 0xFF); CVE-class IRQ handling flaws in drivers. FP: a
correct threaded IRQ with `IRQF_TRIGGER_LOW` and proper EOI must NOT be
flagged.
