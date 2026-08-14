# Evaluation — embedded-interrupt-and-nested

Skill: `skills/embedded/embedded-interrupt-and-nested`.
Stability target: `evaluated`.

## Verification commands (host, GCC 16.1 on PATH)

```
# good examples must exit 0
gcc -Wall -Wextra -Werror -O2 examples/good/critical_section.c -o out && ./out   # 0
gcc -Wall -Wextra -Werror -O2 examples/good/priority_config.c -o out && ./out    # 0
gcc -Wall -Wextra -Werror -O2 examples/good/defer_work.c -o out && ./out         # 0

# bad examples reproduce the rule violation (nonzero exit)
gcc -Wall -Wextra -Werror -O2 examples/bad/shared_var_race.c -o out && ./out          # 1
gcc -Wall -Wextra -Werror -O2 examples/bad/enabling_wrong_priority.c -o out && ./out  # 1
gcc -Wall -Wextra -Werror -O2 examples/bad/blocking_in_isr.c -o out && ./out          # 1
```

The examples are single files that include `../cortex_m_stubs.h`; no extra
flags or libraries are required. The stub models the NVIC/PRIMASK subset the
skill is about: 4-bit priority field, a reserved OS priority floor, a
PRIMASK critical-section mutex, pending bits, and an ISR work budget.

## Synthetic evals

- **easy/negative**: an IRQ enabled at priority 0 (reserved OS range) and at
  priority 16 (outside the 4-bit field) — agent must name both rules and set
  a valid non-reserved priority; `examples/bad/enabling_wrong_priority.c`
  exits 1, `examples/good/priority_config.c` exits 0.
- **medium/negative**: main and an ISR both increment a shared counter
  (load-modify-store) with no critical section — agent must explain why
  `volatile` does not help (lost update is a race) and add
  `__disable_irq`/`__enable_irq` around the RMW; the bad/good pair gives
  deterministic evidence.
- **hard/negative**: an ISR busy-waits for a status bit that a lower-priority
  context must set — agent must identify the single-core deadlock and defer
  the work out of the ISR; `examples/bad/blocking_in_isr.c` trips the budget
  (exit 1) vs `examples/good/defer_work.c` (exit 0).
- **ambiguous**: "which priority is higher" — correct answer is the lower
  number is the higher urgency; 0 is the most urgent configurable value and is
  reserved on an RTOS, so "high priority" does not mean "more important API
  access" (FreeRTOS: only priorities >= the syscall floor may call APIs).

## False-positive evals

- A correct `volatile` word-sized single-writer flag shared with an ISR — must
  NOT be flagged for needing a critical section.
- A valid NVIC priority configuration (in-range, at or below the OS floor, set
  before enable) — must NOT be flagged.
- A minimal ISR that records one event and returns, with the heavy work
  deferred to main via a pending bit — must NOT be flagged as "too slow".
- A balanced `__disable_irq`/`__enable_irq` critical section of short duration
  — must NOT be flagged as "still racy".

## Adversarial evals

- **AD (works in tests, loses updates in the field)**: an ISR that fires after
  main's store in tests, so the shared counter appears correct; the eval
  expects the agent to identify the missing critical section by reordering the
  ISR arrival mid-RMW (`shared_var_race` vs `critical_section` exit codes).
- **AD (enabled without priority)**: an IRQ enabled while its priority still
  defaults to 0 (reset value) — the agent must recognize the 
  enabled-but-unconfigured path and configure priority first; the stub's
  `STUB_FAIL_ENABLE_UNCONFIGURED` catches it.
- **AD (nesting budget)**: an ISR that is correct alone but exceeds the work
  budget under back-to-back arrivals; the agent must keep the ISR short and
  defer, not raise the priority.

## Verified facts (recorded 2026-08-14, GCC 16.1 x86-64 MinGW, `-O2 -Werror`)

1. All 6 examples build clean with `-Wall -Wextra -Werror -O2` and no extra
   flags.
2. bad/enabling_wrong_priority: the stub rejects priority 16 (out of the 4-bit
   field) and priority 0 (reserved OS range); exits 1.
3. bad/blocking_in_isr: the ISR busy-wait charges more than STUB_ISR_BUDGET (8)
   work units; exits 1.
4. good/priority_config: priorities 4/8/8 round-trip through the stub and all
   three IRQs enable; exits 0.
5. good/critical_section: with `__disable_irq`/`__enable_irq` around the RMW,
   the final count equals 2 x ITERATIONS; exits 0.
6. good/defer_work: 8 ISRs each stay within budget, the pending bit requests
   deferred processing, and main drains the work; exits 0.
7. bad/shared_var_race: builds clean with `-Werror`; it is byte-for-byte the
   same code path as good/critical_section except that main's RMW has no
   `__disable_irq`/`__enable_irq`, so the ISR RMW interleaves and updates are
   lost — the final count is below 2 x ITERATIONS and the program returns
   nonzero. Its runtime exit code was not re-confirmed on the host (the
   environment's process spawn failed mid-verification); the good counterpart
   critical_section returned 0 in the same run.

## Documented-as-target (not executed on host)

- Real NVIC entry, stacking, nesting depth on the MSP, entry latency, and
  tail-chaining cannot be exercised on the host. Verify on target under QEMU:
  build a CMSIS Cortex-M3 application for
  `qemu-system-arm -machine mps2-an385` with `arm-none-eabi-gcc` and trigger
  back-to-back interrupts; check `SCB->VTOR`, the vector table words, and
  `DWT->CYCCNT` for latency/tail-chaining per `cmsis` / `arm-arm`.
- FreeRTOS `configMAX_SYSCALL_INTERRUPT_PRIORITY` interaction is a target
  check; the host stub models the reserved floor statically.

## Scoring

- detection: names the rule being violated (RMW race without a critical
  section, out-of-range or reserved priority, busy-wait in an ISR,
  enabled-but-unconfigured IRQ).
- reasoning: separates urgency from magnitude, entry path from priority, and
  `volatile` (access preservation) from critical-section/atomicity
  (serialization); explains nesting as a priority comparison.
- fix: minimal correct change (paired `__disable_irq`/`__enable_irq`, valid
  priority set before enable, defer work out of the ISR); does not over-apply
  atomics or disable interrupts inside the ISR.
- verification: uses the stub exit codes and the deterministic bad/good pair;
  on target, real NVIC/ISR timing checks under QEMU `mps2-an385`.
