# Evaluation — rtos-concurrency-and-isr

Skill: `skills/embedded/rtos-concurrency-and-isr`.
Stability target: `evaluated`.

## Verification commands (host, GCC 16.1 on PATH)

```
# good examples must exit 0
gcc -Wall -Wextra -Werror -O2 examples/good/defer_work_via_queue.c -o out && ./out
gcc -Wall -Wextra -Werror -O2 examples/good/isr_fromisr_apis.c -o out && ./out
gcc -Wall -Wextra -Werror -O2 examples/good/priority_inheritance_mutex.c -o out && ./out
gcc -Wall -Wextra -Werror -O2 examples/good/periodic_delay_until.c -o out && ./out

# bad examples reproduce the rule violation (nonzero exit)
gcc -Wall -Wextra -Werror -O2 examples/bad/blocking_in_isr.c -o out && ./out      # 1
gcc -Wall -Wextra -Werror -O2 examples/bad/isr_calls_task_api.c -o out && ./out   # 1
gcc -Wall -Wextra -Werror -O2 examples/bad/no_deferral_long_isr.c -o out && ./out # 1
gcc -Wall -Wextra -Werror -O2 examples/bad/priority_inversion.c -o out && ./out   # 1
```

The examples are single files that include `../freertos_stubs.h`; no extra
flags or libraries are required.

## Synthetic evals

- **easy/negative**: a correct ISR that sends to a queue with
  `xQueueSendFromISR` and calls `portYIELD_FROM_ISR` — must NOT be flagged;
  the stub shows zero violations and a yield was requested.
- **medium/negative**: a blocking call (`xSemaphoreTake`, `vTaskDelay`,
  `xQueueSend`) inside an ISR — agent must name the rule ("no blocking in ISR
  context") and replace it with the FromISR variant.
- **hard/negative**: a long computation performed inside the ISR instead of
  deferred to a task — agent must identify the latency problem, not just a
  "correct compile".
- **hard/negative**: a binary semaphore used as a mutex — agent must predict
  priority inversion and fix by using `xSemaphoreCreateMutex`, explaining
  priority inheritance.
- **ambiguous**: `vTaskDelayUntil` vs `vTaskDelay` in a periodic task —
  correct answer is "absolute wake time keeps the period fixed; relative delay
  drifts by the processing time".

## False-positive evals

- A correct ISR-to-task handoff using only FromISR APIs and
  `portYIELD_FROM_ISR` — must NOT be flagged.
- A `vTaskDelayUntil` periodic loop with a properly initialized
  `pxPreviousWakeTime` — must NOT be flagged.
- A mutex-guarded critical section with an acknowledged short hold time — must
  NOT be flagged as "still priority inversion".
- A binary semaphore used purely for ISR→task signaling (not exclusion) — must
  NOT be flagged.

## Adversarial evals

- **AD (works in tests, blocks in the field)**: an ISR that calls
  `xQueueSend` when "the queue always has room" — the eval expects
  identification of the non-FromISR API and a fix to `xQueueSendFromISR` with
  `portYIELD_FROM_ISR`.
- **AD (binary semaphore as mutex)**: shared data guarded with a binary
  semaphore; the agent must reproduce the 15-vs-4-tick comparison from the
  good/bad pair as evidence and switch to a mutex.
- **AD (period drift)**: a periodic task using `vTaskDelay` whose period
  stretches as the body grows — the agent must explain the relative-vs-absolute
  difference and switch to `vTaskDelayUntil`, verifying with the 50-vs-60-tick
  stub result.

## Verified facts (recorded 2026-08-14, GCC 16.1 x86-64 MinGW, `-O2 -Werror`)

1. All 8 examples build clean with `-Wall -Wextra -Werror -O2`.
2. bad/blocking_in_isr: stub records both `xSemaphoreTake` and `vTaskDelay`
   called in ISR context; exits 1.
3. bad/isr_calls_task_api: stub records `xQueueSend` (task-context API) in ISR
   context; exits 1.
4. bad/no_deferral_long_isr: ISR accumulates 64 work units (budget 8); exits 1.
5. bad/priority_inversion vs good/priority_inheritance_mutex: the two files
   are identical except for the lock-creation call; the deterministic host
   scheduler model reports the high-priority task finishing at tick 15 (binary
   semaphore, no inheritance) vs tick 4 (mutex, priority inheritance). The bad
   version exits 1, the good version exits 0.
6. good/periodic_delay_until: `vTaskDelayUntil(&last, 10)` over 5 iterations
   with 2 ticks of processing yields exactly 50 ticks (period = 10); the same
   loop with `vTaskDelay(10)` yields 60 ticks (period = 12, drift).
7. good/defer_work_via_queue and good/isr_fromisr_apis: yield was requested
   via `portYIELD_FROM_ISR`, no stub violation, queued data round-trips
   (pointer and int item sizes verified); both exit 0.

## Documented-as-target (not executed on host)

- Real NVIC priorities and `configMAX_SYSCALL_INTERRUPT_PRIORITY` /
  `portASSERT_IF_INTERRUPT_PRIORITY_INVALID` behaviour cannot be simulated on
  the host. Verify on target under QEMU: build the FreeRTOS
  `CORTEX_M3_MPS2_QEMU` demo (Cortex-M3) or a Zephyr application for the
  `qemu_cortex_m3` board and run with `qemu-system-arm -machine mps2-an385`.
- Interrupt stack (MSP) sizing under nesting and real interrupt timing are
  target checks per `cmsis` / `arm-arm`.

## Scoring

- detection: names the wrong-context call (blocking API in ISR, non-FromISR
  API in ISR, long ISR, binary semaphore as mutex, relative delay in a
  periodic task) and the rule it violates.
- reasoning: separates task vs ISR context, signaling vs exclusion, and
  absolute vs relative timing; explains priority inheritance and its absence
  in binary semaphores.
- fix: minimal correct change (FromISR variant + yield, defer to task, mutex,
  `vTaskDelayUntil`); does not over-apply atomics or convert signaling
  semaphores into locks.
- verification: uses the stub exit codes and the deterministic 15-vs-4 and
  50-vs-60 comparisons; on target, real NVIC/ISR timing checks.
