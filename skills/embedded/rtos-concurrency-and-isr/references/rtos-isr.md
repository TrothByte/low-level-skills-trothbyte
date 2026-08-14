# RTOS Concurrency & ISR Discipline — Reference

Sources: FreeRTOS official kernel source and docs (`freertos-docs`; verified
against `queue.c`, `tasks.c`, `task.h`, `queue.h`, `semphr.h` of the
FreeRTOS-Kernel repository), Zephyr kernel services docs (`zephyr-docs`),
Arm CMSIS (`cmsis`), Arm Architecture Reference Manual (`arm-arm`). Claims are
KNOWN where verified against these sources; the host examples are VERIFIED on
GCC 16.1.

## 1. ISRs must be short; defer work to task context

- **RULE**: an ISR preempts all threads and holds the CPU until it returns, so
  it must do the minimum work (acknowledge the interrupt, capture/signal data)
  and hand long processing to a task. Zephyr: "An ISR should execute quickly…
  the ISR should offload some or all processing to a thread"; work that is
  time-consuming or involves blocking "should be handed off to a thread".
- **WHY AI GETS IT WRONG**: the generated code "works", so a long loop or
  parsing inside the ISR looks acceptable; latency and missed deadlines are
  invisible in a happy-path test.
- **CORRECT REASONING**: while an ISR runs, no task executes and (on
  priority-scheduled cores) lower-priority interrupts are masked or deferred.
  A 1 ms ISR on a 1 kHz control loop eats the whole budget. Defer with a queue,
  a work item, or a semaphore/event that wakes a task.
- **EXAMPLE** (bad): ISR computes a checksum over a 64-byte buffer in place
  (`examples/bad/no_deferral_long_isr.c`) — all tasks and lower-priority ISRs
  stall for the whole computation.
- **COUNTEREXAMPLE** (good): ISR copies a pointer into a queue with
  `xQueueSendFromISR` and returns; the consumer task does the work
  (`examples/good/defer_work_via_queue.c`).
- **VERIFICATION**: bound the ISR work measured in the stub: bad exceeds the
  budget and exits 1, good exits 0.
- **SOURCE**: `zephyr-docs` (Interrupts — Offloading ISR Work); `freertos-docs`
  (ISR-safe API, deferred interrupt processing); `arm-arm` (interrupt latency).

## 2. Blocking operations are forbidden in ISRs

- **RULE**: an ISR cannot block: there is no scheduler context to reschedule
  into, and a blocking wait in interrupt context hangs or crashes the system.
  FreeRTOS documents non-ISR APIs with "This function must not be called from
  an interrupt service routine"; the `...FromISR` variants exist precisely
  because they never block.
- **WHY AI GETS IT WRONG**: a blocking call "usually succeeds immediately" in
  tests (resource available), so the latent wait path is never exercised; or
  the agent assumes ISR context can wait "just like a task".
- **CORRECT REASONING**: any call that may wait (`xQueueSend`, `xQueueReceive`,
  `xSemaphoreTake` with a timeout, `vTaskDelay`, `vTaskDelayUntil`,
  `k_sleep`, `k_sem_take` with a non-zero timeout) is illegal from an ISR. The
  ISR-safe forms are non-blocking by construction.
- **EXAMPLE** (bad): ISR calls `xSemaphoreTake(g, portMAX_DELAY)` and
  `vTaskDelay(10)` (`examples/bad/blocking_in_isr.c`) — the stub records both
  as violations; the program exits 1.
- **COUNTEREXAMPLE** (good): ISR uses `xSemaphoreGiveFromISR` /
  `xQueueSendFromISR`, which return immediately
  (`examples/good/isr_fromisr_apis.c`).
- **VERIFICATION**: run the stub: any blocking call while ISR context is active
  increments the violation counter and the example exits nonzero.
- **SOURCE**: `freertos-docs` (queue.h `xQueueSend*` "must not be called from an
  interrupt service routine"; `xQueueGenericSendFromISR` "except without
  blocking"); `zephyr-docs` (Interrupts — "Many kernel APIs can be used only by
  threads, and not by ISRs"; `k_can_yield()` returns false in ISRs).

## 3. Know the executing context: task vs ISR

- **RULE**: task context has a per-task stack and can block/yield; ISR context
  runs on the interrupt stack, cannot block, and (on Cortex-M) is entered with
  a different stack pointer. Code that is context-agnostic must query the
  context (Zephyr `k_is_in_isr()`) before choosing an API.
- **WHY AI GETS IT WRONG**: treats "a function that can be called from both
  contexts" as needing only one implementation, and picks a blocking API.
- **CORRECT REASONING**: a routine callable by both a thread and an ISR must
  branch on context or restrict itself to APIs valid in both. FreeRTOS encodes
  the split in the API names (`...FromISR`); Zephyr provides `k_is_in_isr()`.
  ISRs use their own stack area, so deep recursion in an ISR is unsafe even
  when task stacks are large.
- **EXAMPLE** (bad): a "send packet" helper that calls `xQueueSend` is invoked
  from an ISR (`examples/bad/isr_calls_task_api.c`) — the stub flags the
  non-FromISR call and the program exits 1.
- **COUNTEREXAMPLE** (good): the ISR path calls `xQueueSendFromISR` and yields
  via `portYIELD_FROM_ISR` when a higher-priority task was woken
  (`examples/good/isr_fromisr_apis.c`).
- **VERIFICATION**: the stub knows whether ISR context is active and fails on
  task-context-only APIs; on target, `portASSERT_IF_INTERRUPT_PRIORITY_INVALID`
  / configASSERT catch misuse.
- **SOURCE**: `zephyr-docs` (Interrupts — "An ISR executes in the kernel's
  interrupt context. This context has its own dedicated stack area");
  `freertos-docs` (ISR-safe APIs); `cmsis`/`arm-arm` (Cortex-M MSP/PSP).

## 4. Use FromISR APIs and request a context switch at the end of the ISR

- **RULE**: from an ISR use the `...FromISR` variants
  (`xQueueSendFromISR`, `xSemaphoreGiveFromISR`). They report whether a
  higher-priority task was woken via `pxHigherPriorityTaskWoken`; if so, call
  `portYIELD_FROM_ISR(pxHigherPriorityTaskWoken)` before returning so the
  scheduler switches to the woken task.
- **WHY AI GETS IT WRONG**: the agent calls `xQueueSend` from the ISR "because
  the queue usually has room", or drops the yield so the woken task only runs
  on the next unrelated tick or ISR.
- **CORRECT REASONING**: the FromISR API copies the item and returns a wake
  flag instead of waking a task inline; the ISR must translate that flag into a
  context switch request at ISR exit. Missing the yield = the woken high-
  priority task is delayed until something else reschedules.
- **EXAMPLE** (bad): `xQueueSend(g_queue, &event, 0)` in the ISR — may block,
  and no yield is ever requested (`examples/bad/isr_calls_task_api.c`).
- **COUNTEREXAMPLE** (good): `xQueueSendFromISR(..., &xHigherPriorityTaskWoken);`
  followed by `portYIELD_FROM_ISR(xHigherPriorityTaskWoken);`
  (`examples/good/defer_work_via_queue.c`, `examples/good/isr_fromisr_apis.c`).
- **VERIFICATION**: the good examples assert `stub_yields_requested() > 0`;
  the stub's `portYIELD_FROM_ISR` counts the request.
- **SOURCE**: `freertos-docs` (queue.h `xQueueSendFromISR` doc: "if sending …
  caused a task to unblock, and the unblocked task has a priority higher than
  the currently running task … a context switch should be requested before the
  interrupt is exited"; example uses `portYIELD_FROM_ISR`).

## 5. Priority inversion: use a mutex, not a binary semaphore, for exclusion

- **RULE**: a mutex implements priority inheritance — when a high-priority task
  blocks on a mutex held by a low-priority task, the kernel temporarily raises
  the holder to the waiter's priority, so the holder finishes quickly and the
  high-priority task is unblocked promptly. Binary semaphores do NOT inherit
  priority, so an unrelated medium-priority task can run ahead of the lock
  holder and starve the high-priority waiter (priority inversion).
- **WHY AI GETS IT WRONG**: "mutex and binary semaphore are interchangeable",
  so the agent guards shared data with a binary semaphore and cannot explain
  the random timing stalls.
- **CORRECT REASONING**: mutex = ownership + priority inheritance + (optionally)
  recursion; binary semaphore = signaling only, no owner, no inheritance.
  For mutual exclusion use `xSemaphoreCreateMutex` (FreeRTOS) / `k_mutex`
  (Zephyr). Zephyr limits inheritance via `CONFIG_PRIORITY_CEILING`.
- **EXAMPLE** (bad): lock created with `xSemaphoreCreateBinary`; the host
  scheduler model shows the high-priority task finishing at tick 15 (it waits
  out the medium task's entire run) — `examples/bad/priority_inversion.c`,
  exits 1.
- **COUNTEREXAMPLE** (good): same model with `xSemaphoreCreateMutex`; the
  holder inherits the waiter's priority, finishes immediately, and the
  high-priority task completes at tick 4 — `examples/good/priority_inheritance_mutex.c`,
  exits 0.
- **VERIFICATION**: the two files differ only in the create call; run both and
  compare completion ticks (15 vs 4, deterministic).
- **SOURCE**: `freertos-docs` (semphr.h: binary semaphore "does not use a
  priority inheritance mechanism … see xSemaphoreCreateMutex"; mutex "uses a
  priority inheritance mechanism"; kernel `xTaskPriorityInherit` in `tasks.c`
  applied only to `queueQUEUE_IS_MUTEX` queues); `zephyr-docs` (Mutexes —
  Priority Inheritance, `CONFIG_PRIORITY_CEILING`).

## 6. Semaphores vs mutexes: signaling vs mutual exclusion

- **RULE**: binary/counting semaphores are for signaling (ISR → task wakeup,
  resource counting); mutexes are for mutual exclusion of shared resources.
  Mutexes cannot be used from ISRs; semaphores can be given from ISRs. A
  counting semaphore initialized to N guards N resources; a mutex guards one
  resource with ownership.
- **WHY AI GETS IT WRONG**: uses a binary semaphore for exclusion (no
  inheritance, no owner, "give" from any task allowed) or tries to take a
  mutex from an ISR.
- **CORRECT REASONING**: choose by intent. ISR→task handoff → binary semaphore
  (`xSemaphoreGiveFromISR` / `k_sem_give`); N instances of a resource → counting
  semaphore; exclusive access to one resource → mutex. Zephyr: "Mutex objects
  are not designed for use by ISRs"; a semaphore "may be given by a thread or
  an ISR".
- **EXAMPLE** (bad): a binary semaphore used as a lock in tasks
  (`examples/bad/priority_inversion.c`) — no inheritance, so priority inversion
  occurs.
- **COUNTEREXAMPLE** (good): a mutex for the lock
  (`examples/good/priority_inheritance_mutex.c`) and a binary semaphore only for
  ISR→task signaling (`examples/good/isr_fromisr_apis.c`).
- **VERIFICATION**: host models run both and show the inheritance difference;
  on target, code review checks that the same object is never used both for
  exclusion and for wakeup.
- **SOURCE**: `freertos-docs` (semphr.h mutex/binary/counting docs; "Mutex type
  semaphores cannot be used from within interrupt service routines");
  `zephyr-docs` (Semaphores; Mutexes).

## 7. vTaskDelayUntil vs vTaskDelay: absolute vs relative period

- **RULE**: `vTaskDelay(n)` blocks relative to the moment it is called, so
  processing time and interrupts accumulate as period drift. `vTaskDelayUntil`
  (newer name `xTaskDelayUntil`) blocks to an absolute wake time
  (`*pxPreviousWakeTime + xTimeIncrement`) and updates the wake time itself,
  giving a constant period for periodic tasks.
- **WHY AI GETS IT WRONG**: the agent writes a periodic loop with `vTaskDelay`
  and the period silently stretches as the task body grows; the bug only shows
  as timing jitter under load.
- **CORRECT REASONING**: use `vTaskDelayUntil` for fixed-frequency work. The
  `pxPreviousWakeTime` variable must be initialised with the current tick
  before first use; `xTaskDelayUntil` returns whether the task actually
  delayed (can detect missed deadlines when the next wake time is in the past).
- **EXAMPLE** (bad): `vTaskDelay(10)` after variable 2-tick work → period is 12
  ticks and drifts (`examples/good/periodic_delay_until.c`, `run_drifting`:
  5 x (10 + 2) = 60 ticks).
- **COUNTEREXAMPLE** (good): `vTaskDelayUntil(&last, 10)` → exact 10-tick
  period regardless of work time (5 x 10 = 50 ticks, verified in the example).
- **VERIFICATION**: the stub advances a tick counter; the example asserts
  exactly 50 vs 60 ticks.
- **SOURCE**: `freertos-docs` (task.h: "vTaskDelay() specifies a time at which
  the task wishes to unblock relative to the time at which vTaskDelay() is
  called … xTaskDelayUntil() specifies the absolute (exact) time at which it
  wishes to unblock"; "Calling xTaskDelayUntil with the same xTimeIncrement …
  fixed interface period").

## 8. Stack high-water mark: verify stack sizing at runtime

- **RULE**: `uxTaskGetStackHighWaterMark(task)` returns the minimum free stack
  space (in words) since the task started; a small or zero value means the
  task came close to overflowing. Use it after stress to validate chosen stack
  sizes. Zephyr equivalents: `k_thread_stack_space_get()` and the runtime
  stack-safety feature.
- **WHY AI GETS IT WRONG**: stack sizes are picked "generously" once and never
  measured; a deep call path or a large local array (or deep ISR nesting on the
  shared interrupt stack) overflows in production only.
- **CORRECT REASONING**: check the high-water mark during bring-up under worst
  case (deepest call path, largest locals, ISR nesting); it is the measured
  margin, not the declared size.
- **EXAMPLE** (bad): task declared with 128 words of stack and a
  256-word local buffer — overflow is silent until corruption shows up
  elsewhere.
- **COUNTEREXAMPLE** (good): the periodic example queries
  `uxTaskGetStackHighWaterMark(h)` and fails the run if the margin is zero
  (`examples/good/periodic_delay_until.c`).
- **VERIFICATION**: run the example; the stub returns a simulated margin and
  the run asserts it is nonzero; on target read the real value from the task
  control block.
- **SOURCE**: `freertos-docs` (task.h: "the minimum free stack space there has
  been (in words …) since the task started. The smaller the returned number the
  closer the task has come to overflowing its stack"); `zephyr-docs` (Threads —
  Querying stack usage / Runtime Stack Safety).

## 9. Zephyr: threads and kernel objects

- **RULE**: in Zephyr a thread is `k_thread` (created via `k_thread_create` /
  `K_THREAD_DEFINE` with a `K_THREAD_STACK_DEFINE` stack); kernel objects are
  `k_sem`, `k_mutex`, `k_msgq`, `k_queue`, `k_event`, `k_condvar`. Priorities
  are integers where a LOWER number is higher priority (opposite of FreeRTOS);
  the scheduler runs the highest-priority ready thread and ISRs preempt threads.
- **WHY AI GETS IT WRONG**: the agent carries FreeRTOS priority semantics
  (higher number = higher priority) into Zephyr, or treats ISR handlers as
  threads.
- **CORRECT REASONING**: map FreeRTOS concepts one-to-one but check semantics:
  FreeRTOS `xSemaphoreTake` ↔ Zephyr `k_sem_take`; mutex `k_mutex_lock`; queue
  `k_msgq_put`/`k_queue_append`; ISR handler `my_isr(void *arg)` connected via
  `IRQ_CONNECT`. In Zephyr an ISR may `k_sem_give` but must not wait if the
  semaphore is unavailable.
- **EXAMPLE** (bad): ISR calls `k_sem_take(&s, K_MSEC(50))` — waits in ISR
  context; Zephyr only allows taking a semaphore from an ISR when it will not
  wait.
- **COUNTEREXAMPLE** (good): ISR gives the semaphore
  (`k_sem_give(&s)` in the ISR, `k_sem_take(&s, K_MSEC(50))` in the thread).
- **VERIFICATION**: review against the Zephyr kernel-services pages; use
  `k_is_in_isr()` to make shared helpers context-safe.
- **SOURCE**: `zephyr-docs` (Threads — "A thread is a kernel object that is
  used for application processing that is too lengthy or too complex to be
  performed by an ISR"; Thread Priorities; Semaphores — "The kernel does allow
  an ISR to take a semaphore, however the ISR must not attempt to wait if the
  semaphore is unavailable"; Scheduling — ISRs take precedence over threads).

## 10. Cortex-M ISR context details

- **RULE**: on Cortex-M, thread mode and handler mode use different stack
  pointers: tasks (FreeRTOS ports) run on PSP, ISRs run on MSP
  (the interrupt/main stack), and the NVIC supports nested, priority-ordered
  interrupts. FreeRTOS uses `configMAX_SYSCALL_INTERRUPT_PRIORITY` /
  `portASSERT_IF_INTERRUPT_PRIORITY_INVALID` so that only ISRs at or below the
  max syscall priority may call kernel APIs.
- **WHY AI GETS IT WRONG**: assumes a single stack for everything and ignores
  interrupt priorities, so an ISR at a priority above the syscall ceiling calls
  a kernel API and corrupts kernel state, or the interrupt stack is undersized.
- **CORRECT REASONING**: keep ISR code non-blocking AND at a priority at/below
  the max syscall priority; size the interrupt (MSP) stack for worst-case
  nesting; use CMSIS `NVIC_SetPriority` / `__NVIC_EnableIRQ` and the
  FreeRTOS `portYIELD_FROM_ISR` at the end of ISR handlers.
- **EXAMPLE** (bad): a high-priority NVIC ISR (above the syscall ceiling) calls
  `xQueueSendFromISR`; with configASSERT enabled this triggers an assertion
  failure.
- **COUNTEREXAMPLE** (good): ISR priority configured at/below the ceiling and
  all kernel interaction done via FromISR APIs plus `portYIELD_FROM_ISR`.
- **VERIFICATION**: check the port's ISR entry/exit macros and NVIC priority
  configuration on target; host models cannot emulate NVIC, so this is a
  documented-as-target check.
- **SOURCE**: `cmsis` (NVIC, `__NVIC_EnableIRQ`, core register access);
  `arm-arm` (handler vs thread mode, MSP/PSP, NVIC); `freertos-docs`
  (interrupt priority configuration).

## Verified facts (recorded 2026-08-14, GCC 16.1 x86-64 MinGW, -O2 -Werror)

| Example | Behaviour | Exit |
|---|---|---|
| bad/blocking_in_isr | stub flags xSemaphoreTake + vTaskDelay in ISR | 1 |
| bad/isr_calls_task_api | stub flags xQueueSend (task API) in ISR | 1 |
| bad/no_deferral_long_isr | ISR does 64 work units, budget 8 | 1 |
| bad/priority_inversion | binary semaphore: H finishes tick 15 | 1 |
| good/defer_work_via_queue | ISR queues pointer, yield requested, consumer processes | 0 |
| good/isr_fromisr_apis | FromISR APIs + portYIELD_FROM_ISR, both objects verified | 0 |
| good/priority_inheritance_mutex | mutex: H finishes tick 4 | 0 |
| good/periodic_delay_until | vTaskDelayUntil=50 ticks, vTaskDelay=60, HWM>0 | 0 |

Teaching points:
- The stub enforces the two rules (no blocking in ISR, FromISR-only in ISR)
  with a context flag; the same checks on target come from configASSERT /
  `portASSERT_IF_INTERRUPT_PRIORITY_INVALID` / `k_is_in_isr()`.
- Priority-inversion pair is deterministic (15 vs 4 ticks) and differs only in
  the lock-creation call.
- QEMU target: Zephyr `qemu_cortex_m3` board and the FreeRTOS
  `CORTEX_M3_MPS2_QEMU` demo both run on `-machine mps2-an385`; build as
  documented-as-target.

## Common failure modes

- A12 (blocking in ISR): `xQueueSend`/`xSemaphoreTake`/`vTaskDelay` inside an
  ISR — the stub and configASSERT catch it.
- B14 (no deferral): long ISR work — latency budget blown; measure ISR time.
- B15 (binary semaphore as mutex): priority inversion — use a mutex.
- B16 (missing yield): FromISR call without `portYIELD_FROM_ISR` — woken task
  runs late.
- B17 (relative delay in periodic task): period drift — use `vTaskDelayUntil`.
