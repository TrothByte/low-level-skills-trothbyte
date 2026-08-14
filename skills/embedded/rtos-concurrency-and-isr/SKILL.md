---
name: rtos-concurrency-and-isr
description: Use when writing, reviewing, or debugging FreeRTOS/Zephyr firmware with tasks, queues, semaphores, mutexes, or interrupt handlers — blocking calls in ISRs, ISR-safe FromISR APIs, priority inversion and inheritance, task vs ISR context, periodic task timing, and stack sizing. Teaches context-correct RTOS usage and verification.
---

# RTOS Concurrency & ISR Discipline

## When to use

- Writing or reviewing FreeRTOS or Zephyr code that runs in two contexts:
  tasks and interrupt handlers.
- Deciding which API to call from an ISR (`xQueueSendFromISR` vs
  `xQueueSend`, `portYIELD_FROM_ISR`, `k_sem_give` in ISR context).
- Explaining priority inversion / priority inheritance and choosing between
  a mutex and a binary semaphore.
- Diagnosing periodic-task timing drift (`vTaskDelay` vs `vTaskDelayUntil`)
  or mysterious "stalls" under load.
- Sizing task stacks and validating them with the stack high-water mark.

## When not to use

- Bare-metal code with no RTOS — use `embedded-volatile-and-memory-ordering`.
- Interrupt controller / NVIC configuration, vector tables, nesting rules
  for their own sake — see `embedded-interrupt-and-nested`.
- Multi-core memory ordering between cores — use `memory-ordering-reasoning`.
- Host OS threads / pthreads / condvars — use
  `concurrency-deadlock-and-lock-ordering` or `atomics-c11-cpp11-rust`.

## What the agent often gets wrong

- "Blocking APIs are fine in an ISR if the resource is usually available."
  Blocking is illegal in ISR context regardless of the happy path; the stub
  and configASSERT enforce it.
- "`xQueueSend` and `xQueueSendFromISR` are interchangeable." The FromISR
  variant never blocks and reports a wake flag for the ISR to act on.
- "A mutex and a binary semaphore are the same thing." A binary semaphore
  has no priority inheritance, so guarding data with one can cause priority
  inversion.
- "Periodic work with `vTaskDelay(n)` keeps a fixed period." It drifts by the
  processing time; `vTaskDelayUntil` gives an absolute period.
- "Stack size is just a number to be generous with." A task (or the shared
  interrupt stack) can overflow under worst case; measure the high-water mark.
- "Zephyr priorities work like FreeRTOS priorities." Zephyr: lower number =
  higher priority; FreeRTOS: higher number = higher priority.

## How to reason correctly

1. Identify the executing context first: task, ISR, or context-agnostic
   helper (Zephyr `k_is_in_isr()`). The legal API set follows from context.
2. In an ISR: no blocking, only `...FromISR` APIs, and end with
   `portYIELD_FROM_ISR(pxHigherPriorityTaskWoken)` when a higher-priority task
   was woken. Keep the ISR short; defer work via a queue/semaphore/work item.
3. Pick the object by intent: mutex = exclusive access (ownership + priority
   inheritance), binary semaphore = signaling (ISR→task wakeup), counting
   semaphore = N resources.
4. For periodic tasks use an absolute wake time (`vTaskDelayUntil`), not a
   relative delay, and check whether a deadline was missed.
5. Validate stack sizes with the high-water mark after worst-case stress.
6. Verify on host with the stub examples, then on target (QEMU
   `mps2-an385` with FreeRTOS/Zephyr) under real interrupt timing.

## What to verify

- No blocking API call (queue/semaphore/delay) appears in any ISR.
- Every ISR-side queue/semaphore call is a `...FromISR` variant and the ISR
  acts on `pxHigherPriorityTaskWoken` with `portYIELD_FROM_ISR`.
- Shared resources are guarded by mutexes (priority inheritance), not binary
  semaphores, unless pure signaling is intended.
- Periodic tasks use `vTaskDelayUntil`/`xTaskDelayUntil` (or a Zephyr timer)
  with a correct `pxPreviousWakeTime` initialization.
- Stack sizes are justified by `uxTaskGetStackHighWaterMark` /
  `k_thread_stack_space_get()` data, and the interrupt stack is sized for
  worst-case nesting.

## How to verify

```
# host: bad examples must exit nonzero, good examples exit 0 (GCC 16.1)
gcc -Wall -Wextra -Werror -O2 examples/bad/blocking_in_isr.c -o out && ./out        # 1
gcc -Wall -Wextra -Werror -O2 examples/bad/isr_calls_task_api.c -o out && ./out     # 1
gcc -Wall -Wextra -Werror -O2 examples/bad/no_deferral_long_isr.c -o out && ./out   # 1
gcc -Wall -Wextra -Werror -O2 examples/bad/priority_inversion.c -o out && ./out     # 1
gcc -Wall -Wextra -Werror -O2 examples/good/defer_work_via_queue.c -o out && ./out  # 0
gcc -Wall -Wextra -Werror -O2 examples/good/isr_fromisr_apis.c -o out && ./out      # 0
gcc -Wall -Wextra -Werror -O2 examples/good/priority_inheritance_mutex.c -o out && ./out  # 0
gcc -Wall -Wextra -Werror -O2 examples/good/periodic_delay_until.c -o out && ./out  # 0
```

Target (documented-as-target, QEMU `mps2-an385`): build the FreeRTOS
`CORTEX_M3_MPS2_QEMU` demo or a Zephyr `qemu_cortex_m3` app and run under
`qemu-system-arm -machine mps2-an385` to exercise real interrupt timing and
NVIC priorities.

## Where the knowledge comes from

- FreeRTOS official kernel source and docs — `freertos-docs` (queue.h,
  task.h, semphr.h, tasks.c: FromISR APIs, blocking restrictions, priority
  inheritance, `vTaskDelayUntil`, high-water mark)
- Zephyr kernel services docs — `zephyr-docs` (interrupts, threads,
  scheduling, semaphores, mutexes, `k_is_in_isr`)
- Arm CMSIS — `cmsis` (NVIC, interrupt priorities)
- Arm Architecture Reference Manual — `arm-arm` (handler/thread mode,
  MSP/PSP, interrupt latency)

## Related skills

- `embedded-volatile-and-memory-ordering` — ISR-shared flags, MMIO access
  (require of)
- `memory-ordering-reasoning` — atomics/ordering for multi-core, where
  ISR-to-thread sharing on one core is not enough
- `embedded-interrupt-and-nested` — NVIC, nesting, interrupt priorities
- `concurrency-deadlock-and-lock-ordering` — lock ordering across tasks
- `c-signal-handler-safety` — async-signal-safe context restrictions
  (parallel discipline for host signals)
- `atomics-c11-cpp11-rust` — when shared state needs atomics, not RTOS objects

## Evaluation

- Synthetic: blocking call in ISR, task API in ISR, long ISR work, binary
  semaphore used as a mutex; agent must name the rule and the fix.
- False-positive: a correct ISR using `xQueueSendFromISR` +
  `portYIELD_FROM_ISR`, and a correct `vTaskDelayUntil` periodic loop, must
  NOT be flagged.
- Adversarial: an ISR that "works" in tests (resource available) but can
  block under load; a binary-semaphore "lock" with unexplained timing jitter;
  a periodic task that drifts as work grows. Evidence: stub exit codes and
  the deterministic 15-vs-4-tick priority-inversion comparison.
