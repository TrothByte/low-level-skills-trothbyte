# Evaluation — interrupt-controller-gic-apic

Skill: `skills/kernel/interrupt-controller-gic-apic`. Type: unique.
Stability: researched (Python GIC routing model executed on this host; ARM
GIC/x86 APIC hardware absent — controller behavior is INFERRED/UNVERIFIED).

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| SPI routing to a CPU via distributor | `examples/good/gic_routing_model.py` | CPU0 True, CPU1 False | runs (see facts) |
| Correct SPI number = line + 32 | reasoning case | 37 for line 5 | eval case |
| Edge/level + missing deassert | `examples/bad/edge_level_confusion.py` | FLAG: storm + SGI/SPI mix | runs |
| Missing GIC EOI | reasoning case | FLAG: lost interrupt | eval case |

## False-positive evals (correct code that must NOT be flagged)

- A GICv3 handler that acks with `ICC_IAR1_EL1`, services, and EOIs with
  `ICC_EOIR1_EL1` — correct.
- A threaded IRQ (`IRQF_ONESHOT`) whose thread handler sleeps and the hard
  handler only acks — correct pattern.
- `irq_set_affinity` to a CPU that actually owns the device's target — legal.
- A level IRQ handler that clears the device's pending bit BEFORE EOI —
  correct order.

## Historical evals

- **GIC IRQ storm class** — documented in Linux kernel git: level IRQs
  handled with a missing device-deassert, or a GIC EOI mismatch; agent must
  diagnose the storm as controller semantics, not "too many interrupts".
- **x86 spurious vector 0xFF** — unmasked LAPIC spurious interrupts; agent
  must know the spurious vector exists and must be masked/programmed.
- **CVE-class IRQ handling flaws** — drivers that call `irq_disable`/
  `enable` in the wrong context or miss `irq_set_affinity` migration; agent
  must identify the controller-API misuse.

## Adversarial evals (compiles-but-wrong)

- The bad fixture runs and prints a storm — the model is demonstrably wrong,
  must be flagged.
- Code using x86 EOI (`apic_eoi`) inside a GIC driver — must be rejected.
- An SPI registered with the raw line number (5) instead of 37 — must be
  flagged as an SGI/SPI confusion.

## Verification commands

Host (executed on this host):

```
python3 examples/good/gic_routing_model.py
python3 examples/bad/edge_level_confusion.py
```

Target (documented, toolchain not on this host):

```
qemu-system-aarch64 -machine virt -cpu cortex-a57 -kernel <kernel> -nographic
cat /proc/interrupts          # x86 and ARM (under QEMU/board)
dmesg | grep -i "DMAR-IR|its" # MSI remapping state
```

## Verified facts (KNOWN / INFERRED / UNVERIFIED)

- KNOWN: `gic_routing_model.py` runs on this host and prints CPU0 True /
  CPU1 False / handler calls 1.
- KNOWN: `edge_level_confusion.py` runs and prints the storm count and the
  SGI/SPI confusion — demonstrating both flaws.
- INFERRED: GICv3 requires explicit EOI and uses SPI=32..1019
  (researched from ARM GIC spec; hardware absent).
- INFERRED: x86 spurious vector is 0xFF and IO-APIC RTEs carry trigger +
  vector (researched from `intel-sdm` Vol.3B).
- UNVERIFIED: real GIC/APIC behavior on actual hardware.

## Scoring

- Precision: high for the model-checked properties (SPI range, routing,
  storm). Recall: high for the documented classes; controller-hardware
  specifics are INFERRED. FP-rate: low — correct EOI/ack order is
  unambiguous.

## Tooling availability (honest)

- Available on this host: python 3.11.9 (both fixtures run).
- NOT installed: ARM GIC hardware/QEMU with `virt` machine, x86 with APIC
  access, irqbalance. Target commands documented, not executed here.
