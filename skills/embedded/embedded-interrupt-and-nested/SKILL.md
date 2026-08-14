---
name: embedded-interrupt-and-nested
description: Use when writing, reviewing, or debugging bare-metal Cortex-M firmware that uses interrupts — NVIC enable/pending/priority, exception vector table, nesting and preemption, PRIMASK critical sections with __disable_irq/__enable_irq, interrupt latency, and sharing state between ISR and main context. Teaches Cortex-M interrupt discipline and host verification.
---

# Embedded Interrupts & Nested Preemption (Cortex-M)

## When to use

- Writing or reviewing bare-metal Cortex-M firmware that enables interrupts,
  configures NVIC priorities, or installs IRQ handlers.
- Deciding how to protect a read-modify-write shared between an ISR and the
  main loop (`__disable_irq`/`__enable_irq`, PRIMASK).
- Choosing a priority for a device IRQ or explaining why one ISR preempted
  another (nesting) — or why it did not.
- Debugging "interrupt never fires", "handler runs at the wrong time", or
  "shared counter loses updates".
- Deciding what belongs in an ISR versus deferred to main context, and how to
  request deferred work (NVIC pending, PendSV, software interrupt).

## When not to use

- FreeRTOS/Zephyr task+ISR interaction (queues, FromISR APIs, priority
  inversion) — use `rtos-concurrency-and-isr`.
- Pure volatile/ordering questions for shared flags and MMIO — use
  `embedded-volatile-and-memory-ordering` first; this skill builds on it.
- Host signals (`sigaction`, async-signal-safety) — use
  `c-signal-handler-safety`.
- Multi-core / SMP memory ordering — use `memory-ordering-reasoning`.
- AVR/PIC/other non-Cortex-M cores — the NVIC/PRIMASK specifics do not apply.

## What the agent often gets wrong

- "Lower priority number means less important." On the NVIC the number is the
  urgency: 0 is the highest configurable priority, 15 (4 bits) the lowest.
- "I can pick any priority; the NVIC clamps it." Values wider than the
  implemented priority bits are ignored; the priority field is not full range.
- "Priority 0 is a good choice for my IRQ." Priority 0 is typically reserved
  for the OS tick/PendSV; a device IRQ at 0 preempts the tick and bypasses
  the RTOS critical sections, silently corrupting timekeeping.
- "Enabling an IRQ without a priority is fine." After reset all configurable
  priorities default to 0, so an enabled-but-unconfigured IRQ runs at the
  highest (most disruptive) priority.
- "`volatile` makes the shared counter safe." `count++` is load-modify-store;
  without a critical section the ISR can preempt main between load and store
  and the update is lost. `volatile` only forces the access to memory.
- "The ISR is fast enough; busy-waiting is fine." A spin in the ISR blocks
  every equal/lower-priority interrupt and main; on a single core a condition
  set by a lower-priority context can never become true (deadlock).
- "Disabling interrupts inside an ISR helps pending work run." It restores and
  re-stacks on exit and defeats tail-chaining; the NVIC already pends
  back-to-back exceptions automatically.
- "Higher priority means the ISR may call more APIs." On FreeRTOS the
  opposite: only priorities numerically >= `configMAX_SYSCALL_INTERRUPT_PRIORITY`
  may call kernel APIs; more-urgent ISRs are invisible to the kernel.

## How to reason correctly

1. Trace the entry path: vector table address (VTOR), the word for IRQn,
   handler mode, MSP stacking. If the handler never runs, check the vector
   and VTOR before touching priorities.
2. Read priorities as urgency: lower number = higher urgency; fixed -3/-2/-1
   (Reset/NMI/HardFault) always win; configurable range 0..2^PRIOBITS-1.
3. Decide nesting by comparing priorities, not by "order of configuration":
   a numerically lower priority preempts; equal priorities pend and run in
   exception-number order.
4. For state shared with an ISR ask "who writes, how many words, is it a
   read-modify-write?" Single writer + word-sized flag -> `volatile`.
   RMW or two writers -> critical section (`__disable_irq`/`__enable_irq`) or
   C11 `_Atomic`.
5. Keep the ISR minimal: record the event, set a pending bit or queue, return.
   Defer heavy work to main or a software interrupt (PendSV).
6. Configure priority before enabling the IRQ, respect the reserved OS floor,
   and keep critical sections short and balanced.
7. Verify on the host with the stub examples, then on target under QEMU
   `mps2-an385` with real interrupt timing.

## What to verify

- Every IRQ handler has a vector-table entry at the correct offset and the
  table address matches `SCB->VTOR`.
- Every IRQ has a priority set in the valid range before enable, and no device
  IRQ sits below the reserved OS floor.
- Every RMW on state shared with an ISR is inside a paired
  `__disable_irq`/`__enable_irq` critical section (or uses `_Atomic`).
- No busy-wait or long loop in any ISR; ISR work is charged against a budget.
- Critical sections are balanced on all paths (no `__enable_irq` without a
  matching disable, no nested disable).
- Shared flags are `volatile`; ordering requirements are explicit (barriers)
  per `embedded-volatile-and-memory-ordering`.

## How to verify

```
# host, GCC 16.1: bad examples exit nonzero, good examples exit 0
gcc -Wall -Wextra -Werror -O2 examples/bad/shared_var_race.c -o out && ./out          # 1
gcc -Wall -Wextra -Werror -O2 examples/bad/enabling_wrong_priority.c -o out && ./out  # 1
gcc -Wall -Wextra -Werror -O2 examples/bad/blocking_in_isr.c -o out && ./out          # 1
gcc -Wall -Wextra -Werror -O2 examples/good/critical_section.c -o out && ./out        # 0
gcc -Wall -Wextra -Werror -O2 examples/good/priority_config.c -o out && ./out         # 0
gcc -Wall -Wextra -Werror -O2 examples/good/defer_work.c -o out && ./out              # 0
```

Target (documented-as-target): build with `arm-none-eabi-gcc` for
`qemu-system-arm -machine mps2-an385` to exercise real NVIC entry, nesting,
latency, and tail-chaining; CMSIS device startup + vector table required.

## Where the knowledge comes from

- Arm CMSIS — `cmsis` (NVIC, `SCB->VTOR`, `__disable_irq`/`__enable_irq`,
  PRIMASK/BASEPRI intrinsics, vector table, priority layout)
- Arm Architecture Reference Manual — `arm-arm` (exception model, stacking,
  EXC_RETURN, nesting, tail-chaining, entry latency)
- FreeRTOS docs — `freertos-docs` (interrupt priority rules vs
  `configMAX_SYSCALL_INTERRUPT_PRIORITY`, ISR deferral, PendSV)
- ISO C11 N1570 — `iso-c11-n1570` (volatile vs atomicity, §5.1.2.4, §7.17)
- QEMU docs — `qemu-docs` (mps2-an385 target verification)

## Related skills

- `embedded-volatile-and-memory-ordering` — volatile/atomicity of ISR-shared
  state (require of)
- `rtos-concurrency-and-isr` — FromISR APIs, RTOS priority rules (uses this skill)
- `c-signal-handler-safety` — host-signal analogue of ISR discipline
- `memory-ordering-reasoning` — multi-core ordering, where one core is not enough

## Evaluation

- Synthetic: shared counter RMW with and without a critical section; wrong NVIC
  priority (out of range, reserved 0); busy-wait in an ISR; correct deferral.
  Agent must name the rule and the fix; stub exit codes distinguish good/bad.
- False-positive: correct `volatile` flag sharing, valid priority configuration,
  a minimal ISR that only records an event and returns — must NOT be flagged.
- Adversarial: the RMW race "works" in tests (the ISR fires after the store)
  but loses updates when the interrupt lands mid-store; an IRQ enabled without
  a priority; an ISR that busy-waits for a lower-priority resource (deadlock).
  Evidence is the deterministic bad/good stub pair in `examples/`.
