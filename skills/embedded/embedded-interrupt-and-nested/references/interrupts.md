# Interrupts, Nesting, and Critical Sections on Cortex-M — Reference

Sources: Arm CMSIS-5 (`cmsis`, core_cm*.h, startup files); Arm Architecture
Reference Manual (`arm-arm`, exception model, stacking, EXC_RETURN,
tail-chaining); FreeRTOS docs (`freertos-docs`, interrupt priority rules,
PendSV, deferral); ISO C11 N1570 (`iso-c11-n1570`, volatile vs atomicity).

## 1. Exception entry, the vector table, and MSP stacking

- **RULE**: on Cortex-M the vector table (located by `SCB->VTOR`, reset
  default `0x00000000`) holds the initial Main Stack Pointer at word 0 and the
  reset handler at word 1; device IRQ number N has its handler at word 16 + N.
  On exception entry the hardware automatically pushes r0-r3, r12, LR, PC,
  xPSR (8 words) onto the active stack — MSP in handler mode — reads the
  vector, and switches to handler mode. Interrupt code is invoked by the
  vector, not by a poll.
- **WHY AI GETS IT WRONG**: models interrupts as function calls or polling and
  forgets that the entry address comes from the vector table, so a wrong or
  missing vector (or wrong VTOR) makes the handler silently never run, or run
  the wrong handler, and the agent blames the priority settings.
- **CORRECT REASONING**: the vector word is an address fetched by hardware and
  populated at link time by the startup file; the handler runs in handler mode
  on the MSP with the frame already pushed, and returns via `BX LR` with an
  EXC_RETURN value, not a plain return.
- **EXAMPLE (bad)**: the IRQ handler symbol is renamed but the startup file is
  not regenerated; the vector word still points at the old name (or zeros), the
  interrupt HardFaults, and "the priority must be wrong" is the wrong diagnosis.
- **COUNTEREXAMPLE (good)**: vector table regenerated from final symbols,
  `VTOR` left at reset default or explicitly set to the linker's vector
  section, and the vector word verified with `arm-none-eabi-objdump -D`.
- **VERIFICATION**: on target, trigger the interrupt and break in the handler;
  check `SCB->VTOR` and the vector word; on host this is a target check.
- **SOURCE**: `cmsis` (startup_<device>.s, `SCB->VTOR`, core_cm*.h vector
  layout); `arm-arm` (exception entry, stacking, EXC_RETURN).

## 2. Exception priorities: fixed negatives and the configurable range

- **RULE**: Reset, NMI, and HardFault have fixed priorities -3, -2, -1 and
  always win. Every other exception (SVCall, PendSV, SysTick, all device IRQs)
  uses a configurable priority in 0..2^PRIOBITS-1, where PRIOBITS is the
  implemented width of the NVIC priority field (typically 4 on Cortex-M3/M4).
  Lower number = higher urgency. With equal priorities the exception with the
  lower exception number runs first.
- **WHY AI GETS IT WRONG**: reads "priority" like a task priority ("10 is more
  important than 4"); on the NVIC the arithmetic is inverted, so the "more
  important" IRQ is actually the less urgent one and nesting behavior is the
  opposite of what was intended.
- **CORRECT REASONING**: compare urgency, not magnitude: a device at priority
  4 preempts a device at priority 8. Fixed negative priorities outrank all
  configurable ones, so no software-configurable IRQ can preempt NMI or
  HardFault.
- **EXAMPLE (bad)**: two IRQs are set to 0 and 8 intending "8 is the
  important one"; the priority-0 IRQ (unintended) preempts the other on every
  arrival and its long handler jams the system.
- **COUNTEREXAMPLE (good)**: the time-critical IRQ gets the numerically lower
  value and the shorter handler; a less urgent IRQ gets a higher number.
- **VERIFICATION**: read back with `NVIC_GetPriority` (stub: `nvic_get_priority`)
  and compare relative ordering; a 4-bit part accepts 0..15 only.
- **SOURCE**: `cmsis` (`__NVIC_PRIO_BITS`, `NVIC_SetPriority`);
  `arm-arm` (priority model).

## 3. Nesting and preemption

- **RULE**: the NVIC preempts a running ISR only when the arriving exception
  has a strictly lower priority number. Equal-priority IRQs do not preempt;
  they pend and run after the current one (in exception-number order). Nested
  ISRs stack their frames on the MSP, so the MSP must be sized for the
  worst-case nesting depth, not just one frame.
- **WHY AI GETS IT WRONG**: assumes "an interrupt arriving during an ISR
  always interrupts it" (true only when it is strictly more urgent) and sizes
  the stack for a single handler, so a nested burst overflows the MSP.
- **CORRECT REASONING**: nesting is a property of the priority comparison, not
  of "how many interrupts are enabled". On an RTOS, an IRQ more urgent than
  `configMAX_SYSCALL_INTERRUPT_PRIORITY` also preempts the kernel's own
  critical sections, so it cannot call kernel APIs.
- **EXAMPLE (bad)**: SysTick (tick) and a UART IRQ share the default priority
  0; the UART never preempts the tick and timing is stretched, or the MSP
  overflows when both nest.
- **COUNTEREXAMPLE (good)**: tick at 8, UART at 4; the UART nests into the
  tick only when strictly more urgent, and the MSP is sized for
  "UART nests into tick nests into main" (worst case).
- **VERIFICATION**: host stub checks priority comparisons and ISR depth;
  on target, QEMU `mps2-an385` with both IRQs firing under load.
- **SOURCE**: `arm-arm` (preemption); `cmsis` (NVIC); `freertos-docs`
  (`configMAX_SYSCALL_INTERRUPT_PRIORITY`, interrupt stack sizing).

## 4. Priority configuration: range, defaults, reserved floor

- **RULE**: NVIC priority registers hold the value in the top bits of the
  byte; a value wider than PRIOBITS is truncated/ignored, not clamped to the
  maximum. After reset all configurable priorities read 0, so an IRQ enabled
  without a configured priority runs at 0 — the highest, most disruptive
  urgency. On FreeRTOS, priorities numerically below
  `configMAX_SYSCALL_INTERRUPT_PRIORITY` are reserved for the kernel (tick,
  PendSV) and device IRQs must not use them.
- **WHY AI GETS IT WRONG**: "16 is more urgent than 15 so the NVIC should
  honor it" (it does not fit a 4-bit field), and "priority 0 is the most
  responsive for my device" (it is exactly what an OS reserves so its
  critical sections stay intact).
- **CORRECT REASONING**: set a valid in-range value, keep device IRQs at or
  below the reserved floor, and configure priority before enabling the IRQ.
  CMSIS `NVIC_SetPriority` handles the left-alignment for you.
- **EXAMPLE (bad)**: `nvic_set_priority(IRQ_UART, 16)` and
  `nvic_set_priority(IRQ_TIMER, 0)`; the first is out of the 4-bit field and
  ignored, the second lands in the reserved OS range (stub: exit 1).
- **COUNTEREXAMPLE (good)**: device IRQs at 4 and 8, tick at 2 (floor),
  priority set before `NVIC_EnableIRQ` (stub: exit 0).
- **VERIFICATION**: `examples/bad/enabling_wrong_priority.c` exits 1,
  `examples/good/priority_config.c` exits 0; on target use `NVIC_GetPriority`.
- **SOURCE**: `cmsis` (`NVIC_SetPriority` left-shift, reset values);
  `freertos-docs` (`configMAX_SYSCALL_INTERRUPT_PRIORITY`).

## 5. Critical sections: PRIMASK and BASEPRI

- **RULE**: `__disable_irq()` sets PRIMASK=1, masking every exception with a
  configurable priority (all device IRQs, SysTick, PendSV); only NMI and
  HardFault still arrive. `__enable_irq()` clears it. The pair must be
  balanced on every path and the section kept short. BASEPRI is the finer
  alternative: it masks only exceptions with priority number >= the BASEPRI
  value, letting more-urgent IRQs through.
- **WHY AI GETS IT WRONG**: treats `__disable_irq` as optional ("the race
  never happens in tests"), calls `__enable_irq` on a path where disable was
  skipped, or nests disables without counting; it also forgets PRIMASK does
  not mask NMI/HardFault, so a handler runnable from those contexts must not
  rely on it.
- **CORRECT REASONING**: a critical section makes the load-modify-store atomic
  with respect to configurable-priority interrupts by delaying them until
  `__enable_irq`; the section is also the mechanism behind RTOS lock macros,
  so a long or unbalanced section stalls the whole system.
- **EXAMPLE (bad)**: `examples/bad/shared_var_race.c` — main's `count++` has
  no `__disable_irq`/`__enable_irq`; the ISR lands between load and store and
  the final count is below expected (exit 1).
- **COUNTEREXAMPLE (good)**: `examples/good/critical_section.c` — the same RMW
  wrapped in a paired critical section; the ISR stays pended during the
  section and the count is exact (exit 0).
- **VERIFICATION**: run the pair; also compile with `-Wall -Wextra -Werror`
  and inspect `-O2` asm that the load and store straddle the disable/enable.
- **SOURCE**: `cmsis` (core_cm*.h `__disable_irq`/`__enable_irq`,
  `__set_BASEPRI`); `arm-arm` (PRIMASK/BASEPRI); `iso-c11-n1570`
  (§5.1.2.4 data races — a lost update is a data race, not a volatile fix).

## 6. Interrupt latency

- **RULE**: Cortex-M3/M4 interrupt entry takes about 12 cycles (zero wait
  state) for vector fetch and stacking, and a late-arriving higher-priority
  exception shortens that; every instruction executed inside an ISR adds
  latency for all less-urgent interrupts and for main. Latency budgets are
  met by short ISRs, not by faster priorities.
- **WHY AI GETS IT WRONG**: measures "time from interrupt to handler start"
  against the ISR's own work and misses that the ISR's length is what delays
  every other interrupt; it also adds entry/exit work (e.g. unbalanced
  critical sections) that re-stacks frames.
- **CORRECT REASONING**: keep the ISR to record-and-return; do heavy work in
  main or a software interrupt. Priority only decides ordering, it does not
  shorten the work already running.
- **EXAMPLE (bad)**: a UART ISR that parses a protocol in place; every frame
  stalls the tick and a button IRQ for tens of microseconds.
- **COUNTEREXAMPLE (good)**: the ISR copies the byte and pends a software
  interrupt; parsing runs later in main/PendSV, so the ISR is a few cycles.
- **VERIFICATION**: `examples/bad/blocking_in_isr.c` trips the ISR work
  budget (exit 1); on target, toggle a GPIO in the ISR and measure with a
  logic analyzer or `DWT->CYCCNT`.
- **SOURCE**: `arm-arm` (entry latency, late arrival); `cmsis` (DWT cycle
  counter); `freertos-docs` (interrupt priority vs latency).

## 7. Tail-chaining and late arrival

- **RULE**: when an ISR returns and another exception is already pending, the
  core tail-chains: it skips the unstacking and re-stacking of the returning
  frame and executes the next handler immediately, saving the entry cost.
  This is automatic hardware behavior.
- **WHY AI GETS IT WRONG**: "re-enabling interrupts at the end of the ISR
  lets the pending interrupt run sooner" — it actually forces a full
  return+entry (restore frame, check, push frame), defeating tail-chaining,
  and can reintroduce reentrancy bugs.
- **CORRECT REASONING**: leave the ISR return path clean (no disable/enable
  pairs around the end); the NVIC already pends and chains back-to-back
  exceptions with minimal overhead. Nesting is handled by priorities, not by
  manual re-enabling.
- **EXAMPLE (bad)**: an ISR ends with `__enable_irq(); return;` to "flush"
  a pending IRQ — each back-to-back pair now pays a full unstack+stack.
- **COUNTEREXAMPLE (good)**: the ISR returns without touching PRIMASK; two
  pending IRQs tail-chain and both complete in roughly one entry cost.
- **VERIFICATION**: on target, time two back-to-back IRQs with `DWT->CYCCNT`
  with and without the manual re-enable; compare cycles.
- **SOURCE**: `arm-arm` (tail-chaining, late arrival); `cmsis` (DWT).

## 8. Sharing state between ISR and main context

- **RULE**: a word-sized flag written by one side and read by the other on a
  single core needs `volatile`; a read-modify-write (`count++`), a two-writer
  protocol, or a multi-word structure needs a critical section
  (`__disable_irq`/`__enable_irq`) or C11 `_Atomic`. `volatile` is not atomic
  and does not serialize load-modify-store.
- **WHY AI GETS IT WRONG**: "volatile stops the compiler caching it, so the
  counter is safe" — the load and the store are still two separate operations
  and the ISR can land between them; the update is lost and the behavior is a
  data race (UB in C11 terms).
- **CORRECT REASONING**: decide by writers, not by "shared": one writer +
  word reads/writes -> `volatile`; RMW or multiple writers -> critical section
  or atomics. The main-context side of the RMW must mask interrupts; the ISR
  side of a same-priority RMW cannot be preempted by itself, but can be
  preempted by a more-urgent ISR sharing the same object — then that ISR needs
  the critical section too.
- **EXAMPLE (bad)**: `examples/bad/shared_var_race.c` — main and the ISR both
  increment a shared counter with a wide load-usleep-store; updates are lost
  (exit 1).
- **COUNTEREXAMPLE (good)**: `examples/good/critical_section.c` — main wraps
  the RMW in `__disable_irq`/`__enable_irq`; the count is exact (exit 0).
- **VERIFICATION**: run the pair; `gcc -O2 -S` shows the volatile load/store
  hitting memory while the critical section serializes them.
- **SOURCE**: `iso-c11-n1570` (§5.1.2.4 data races, §7.17 atomics);
  `cmsis` (PRIMASK intrinsics); `embedded-volatile-and-memory-ordering`.

## 9. Deferring work out of the ISR

- **RULE**: an ISR must be minimal: record the event (flag, counter, queue
  item) and return; heavy or non-deterministic work runs in main context, a
  lower-priority IRQ, or a software interrupt (PendSV). On FreeRTOS only
  `...FromISR` APIs are legal in ISR context, and a yielded higher-priority
  task is handled by `portYIELD_FROM_ISR`.
- **WHY AI GETS IT WRONG**: implements protocol logic, parsing, or a
  busy-wait directly in the ISR "because it must react quickly"; the long ISR
  then blocks every equal/lower-priority interrupt and the main loop, and a
  busy-wait for a lower-priority resource is a single-core deadlock.
- **CORRECT REASONING**: reactivity comes from the short ISR firing on time,
  not from doing more work inside it; defer and batch. A spin that waits for a
  flag set by main can never see it while it holds the core.
- **EXAMPLE (bad)**: `examples/bad/blocking_in_isr.c` — the ISR spins on a
  device status bit that nothing can set while it spins; the stub budget check
  trips (exit 1).
- **COUNTEREXAMPLE (good)**: `examples/good/defer_work.c` — the ISR records
  the event, requests a software interrupt via `nvic_set_pending`, and
  returns; main drains the work later (exit 0).
- **VERIFICATION**: run the pair; on target, measure ISR duration and main-loop
  jitter with and without deferral.
- **SOURCE**: `freertos-docs` (FromISR APIs, deferral, PendSV);
  `cmsis` (software interrupts, PendSV).

## 10. NVIC enable, pending, and software interrupts

- **RULE**: an IRQ that arrives while disabled or masked is latched in the
  pending state and fires once enabled/unmasked. `NVIC_EnableIRQ`,
  `NVIC_DisableIRQ`, `NVIC_SetPendingIRQ`, `NVIC_ClearPendingIRQ`,
  `NVIC_GetPendingIRQ`, and `NVIC_GetActive` manage this state; setting a
  pending bit from software is the canonical way to request deferred work or
  a software interrupt.
- **WHY AI GETS IT WRONG**: assumes a disable before the event means the event
  is lost (it is pended, not dropped), or that clearing pending of a still
  active hardware source stops further interrupts (the source re-pends).
- **CORRECT REASONING**: pending survives disable and masking; enable/priority
  configuration must precede use, and clearing pending should be done for a
  source whose condition has been serviced, not while it is still asserting.
- **EXAMPLE (bad)**: the handler clears pending at the top while the device
  is still asserting, so the IRQ re-pends immediately and the CPU is stuck in
  a tight ISR loop.
- **COUNTEREXAMPLE (good)**: the handler services the device (which deasserts
  the source), then clears any residual pending once, and optionally pends a
  different IRQ to defer work.
- **VERIFICATION**: host stub models pending bit set/clear/get
  (`nvic_set_pending`/`nvic_clear_pending`/`nvic_get_pending`);
  on target verify with the NVIC pending registers.
- **SOURCE**: `cmsis` (NVIC functions, NVIC_Pend/En/Dis registers);
  `arm-arm` (pending state model).

## Failure modes mapped to evals

- F1 lost-update race: shared RMW without a critical section
  (`examples/bad/shared_var_race.c` vs `examples/good/critical_section.c`).
- F2 wrong priority: out-of-range or reserved-priority enable
  (`examples/bad/enabling_wrong_priority.c` vs `examples/good/priority_config.c`).
- F3 blocking ISR: busy-wait / oversize ISR work
  (`examples/bad/blocking_in_isr.c` vs `examples/good/defer_work.c`).
