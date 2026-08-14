# embedded — Skills

Embedded development runs on bare metal with MMIO, interrupts, and linker scripts. These skills cover volatile/memory ordering, interrupts and nesting, linker scripts, MPU/TrustZone security, and RTOS ISR discipline.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `embedded-interrupt-and-nested` | Use when writing, reviewing, or debugging bare-metal Cortex-M firmware that uses interrupts — NVIC enable/pending/priority, exception vector table, nesting and preemption, PRIMASK critical sections with __disable_irq/__enable_irq, interrupt latency, and sharing state between ISR and main context. Teaches Cortex-M interrupt discipline and host verification. | source-backed | `skills/embedded/embedded-interrupt-and-nested` |
| `embedded-linker-script` | Use when writing, reviewing, or debugging a bare-metal embedded GNU ld linker script — MEMORY regions, SECTIONS placement, FLASH/RAM mapping, KEEP() on the vector table, startup copy loops with __etext/__data_start/__bss_start, ALIGN(), the location counter, and why firmware fails to boot or starts with stale .data. | source-backed | `skills/embedded/embedded-linker-script` |
| `embedded-mpu-trustzone` | Use when writing, reviewing, or debugging Cortex-M firmware that configures the MPU (ARMv7-M RBAR/RASR or ARMv8-M RBAR/RLAR regions, PRIVDEFENA background region, AP privilege) or ARMv8-M TrustZone (SAU regions, NSC veneers, secure/non-secure transitions, flash/SRAM partitioning), or when secure software faults on non-secure calls. | source-backed | `skills/embedded/embedded-mpu-trustzone` |
| `embedded-volatile-and-memory-ordering` | Use when writing, reviewing, or debugging embedded C that accesses memory-mapped I/O (MMIO) registers or shares flags with an interrupt handler — volatile vs non-volatile access, why -O2 changes behavior, why volatile is not atomic, device vs normal memory attributes, and barriers for ordering. Teaches volatile rules and verification. | source-backed | `skills/embedded/embedded-volatile-and-memory-ordering` |
| `rtos-concurrency-and-isr` | Use when writing, reviewing, or debugging FreeRTOS/Zephyr firmware with tasks, queues, semaphores, mutexes, or interrupt handlers — blocking calls in ISRs, ISR-safe FromISR APIs, priority inversion and inheritance, task vs ISR context, periodic task timing, and stack sizing. Teaches context-correct RTOS usage and verification. | source-backed | `skills/embedded/rtos-concurrency-and-isr` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
