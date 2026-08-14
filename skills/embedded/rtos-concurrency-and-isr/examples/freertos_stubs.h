/* freertos_stubs.h — host-compilable stubs for the FreeRTOS API subset used by
 * the examples. The stubs model the two rules the skill is about: blocking APIs
 * may not be called in ISR context, and ISR-safe (FromISR) variants exist for
 * ISR use. Real queue items are copied byte-for-byte (FreeRTOS queues by copy),
 * so the examples verify queueing semantics on the host. */
#ifndef FREERTOS_STUBS_H
#define FREERTOS_STUBS_H

#include <stdlib.h>
#include <string.h>

typedef int BaseType_t;
typedef unsigned long TickType_t;
typedef unsigned long UBaseType_t;
typedef unsigned long configSTACK_DEPTH_TYPE;
typedef void *QueueHandle_t;
typedef void *SemaphoreHandle_t;
typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

#define pdTRUE        ((BaseType_t)1)
#define pdFALSE       ((BaseType_t)0)
#define pdPASS        ((BaseType_t)1)
#define pdFAIL        ((BaseType_t)0)
#define errQUEUE_FULL ((BaseType_t)0)
#define errQUEUE_EMPTY ((BaseType_t)0)
#define portMAX_DELAY ((TickType_t)0xFFFFFFFFUL)

/* Simulated scheduler state: ISR-context flag, rule-violation counter,
 * yield-request counter and a tick counter. */
struct stub_state_t {
    BaseType_t isr_active;
    int fail_count;
    int yield_count;
    TickType_t tick;
};

static struct stub_state_t g_stub;

#define STUB_MAX_SLOTS 8u
#define STUB_MAX_ITEM_SIZE 32u

typedef struct {
    unsigned char items[STUB_MAX_SLOTS][STUB_MAX_ITEM_SIZE];
    unsigned char item_size;
    unsigned char length;
    unsigned char count;
    unsigned char head;
} stub_queue_t;

typedef struct {
    unsigned count;
    unsigned limit;
    unsigned is_mutex;
} stub_sem_t;

static inline void stub_enter_isr(void) { g_stub.isr_active = pdTRUE; }
static inline void stub_exit_isr(void)  { g_stub.isr_active = pdFALSE; }
static inline BaseType_t stub_isr_active(void) { return g_stub.isr_active; }
static inline int stub_failed(void) { return g_stub.fail_count; }
static inline int stub_yields_requested(void) { return g_stub.yield_count; }

static inline void stub_mark_fail(const char *what) {
    (void)what;
    g_stub.fail_count++;
}

static inline TickType_t xTaskGetTickCount(void) { return g_stub.tick; }

static inline void vTaskDelay(TickType_t xTicksToDelay) {
    if (stub_isr_active() != pdFALSE) {
        stub_mark_fail("vTaskDelay in ISR");
        return;
    }
    g_stub.tick += xTicksToDelay;
}

static inline void vTaskDelayUntil(TickType_t *pxPreviousWakeTime,
                                   TickType_t xTimeIncrement) {
    TickType_t xTimeToWake;
    if (stub_isr_active() != pdFALSE) {
        stub_mark_fail("vTaskDelayUntil in ISR");
        return;
    }
    xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;
    if (g_stub.tick < xTimeToWake) {
        g_stub.tick = xTimeToWake;
    }
    *pxPreviousWakeTime = xTimeToWake;
}

static inline QueueHandle_t xQueueCreate(unsigned uxQueueLength,
                                         unsigned uxItemSize) {
    stub_queue_t *q = (stub_queue_t *)malloc(sizeof(*q));
    if (q == NULL) {
        return NULL;
    }
    memset(q, 0, sizeof(*q));
    q->length = (unsigned char)(uxQueueLength < STUB_MAX_SLOTS ? uxQueueLength : STUB_MAX_SLOTS);
    q->item_size = (unsigned char)(uxItemSize < STUB_MAX_ITEM_SIZE ? uxItemSize : STUB_MAX_ITEM_SIZE);
    return (QueueHandle_t)q;
}

static inline int stub_queue_full(const stub_queue_t *q) {
    return q->count >= q->length;
}

static inline int stub_queue_empty(const stub_queue_t *q) {
    return q->count == 0u;
}

static inline void stub_queue_push(stub_queue_t *q, const void *item) {
    unsigned tail = (unsigned)((q->head + q->count) % q->length);
    memcpy(q->items[tail], item, q->item_size);
    q->count++;
}

static inline void stub_queue_pop(stub_queue_t *q, void *item) {
    memcpy(item, q->items[q->head], q->item_size);
    q->head = (unsigned char)((q->head + 1u) % q->length);
    q->count--;
}

static inline BaseType_t xQueueSend(QueueHandle_t xQueue,
                                    const void *pvItemToQueue,
                                    TickType_t xTicksToWait) {
    stub_queue_t *q = (stub_queue_t *)xQueue;
    (void)xTicksToWait;
    if (stub_isr_active() != pdFALSE) {
        stub_mark_fail("xQueueSend in ISR");
        return pdFAIL;
    }
    if (stub_queue_full(q)) {
        return pdFAIL;   /* would block */
    }
    stub_queue_push(q, pvItemToQueue);
    return pdTRUE;
}

static inline BaseType_t xQueueSendToBack(QueueHandle_t xQueue,
                                          const void *pvItemToQueue,
                                          TickType_t xTicksToWait) {
    return xQueueSend(xQueue, pvItemToQueue, xTicksToWait);
}

static inline BaseType_t xQueueSendFromISR(QueueHandle_t xQueue,
                                           const void *pvItemToQueue,
                                           BaseType_t *pxHigherPriorityTaskWoken) {
    stub_queue_t *q = (stub_queue_t *)xQueue;
    int was_empty = stub_queue_empty(q);
    if (stub_queue_full(q)) {
        return pdFAIL;   /* non-blocking: returns immediately */
    }
    stub_queue_push(q, pvItemToQueue);
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = was_empty ? pdTRUE : pdFALSE;
    }
    return pdPASS;
}

static inline BaseType_t xQueueReceive(QueueHandle_t xQueue,
                                       void *pvBuffer,
                                       TickType_t xTicksToWait) {
    stub_queue_t *q = (stub_queue_t *)xQueue;
    (void)xTicksToWait;
    if (stub_isr_active() != pdFALSE) {
        stub_mark_fail("xQueueReceive in ISR");
        return pdFAIL;
    }
    if (stub_queue_empty(q)) {
        return pdFAIL;   /* would block */
    }
    stub_queue_pop(q, pvBuffer);
    return pdTRUE;
}

static inline BaseType_t xQueueReceiveFromISR(QueueHandle_t xQueue,
                                              void *pvBuffer,
                                              BaseType_t *pxHigherPriorityTaskWoken) {
    stub_queue_t *q = (stub_queue_t *)xQueue;
    if (stub_queue_empty(q)) {
        return pdFAIL;
    }
    stub_queue_pop(q, pvBuffer);
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return pdPASS;
}

static inline SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    stub_sem_t *s = (stub_sem_t *)malloc(sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    s->count = 0u;
    s->limit = 1u;
    s->is_mutex = 0u;
    return (SemaphoreHandle_t)s;
}

static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    stub_sem_t *s = (stub_sem_t *)malloc(sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    s->count = 1u;
    s->limit = 1u;
    s->is_mutex = 1u;
    return (SemaphoreHandle_t)s;
}

static inline int stub_semaphore_is_mutex(SemaphoreHandle_t h) {
    return ((stub_sem_t *)h)->is_mutex;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
    stub_sem_t *s = (stub_sem_t *)xSemaphore;
    if (stub_isr_active() != pdFALSE) {
        stub_mark_fail("xSemaphoreGive in ISR");
        return pdFAIL;
    }
    if (s->count < s->limit) {
        s->count++;
    }
    return pdTRUE;
}

static inline BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore,
                                               BaseType_t *pxHigherPriorityTaskWoken) {
    stub_sem_t *s = (stub_sem_t *)xSemaphore;
    if (s->count < s->limit) {
        s->count++;
    }
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdTRUE;
    }
    return pdPASS;
}

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore,
                                        TickType_t xTicksToWait) {
    stub_sem_t *s = (stub_sem_t *)xSemaphore;
    (void)xTicksToWait;
    if (stub_isr_active() != pdFALSE) {
        stub_mark_fail("xSemaphoreTake in ISR");
        return pdFAIL;
    }
    if (s->count == 0u) {
        return pdFAIL;   /* would block */
    }
    s->count--;
    return pdTRUE;
}

static inline BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t xSemaphore,
                                               BaseType_t *pxHigherPriorityTaskWoken) {
    stub_sem_t *s = (stub_sem_t *)xSemaphore;
    if (s->count == 0u) {
        return pdFAIL;
    }
    s->count--;
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return pdPASS;
}

static inline BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                                     const char *pcName,
                                     configSTACK_DEPTH_TYPE usStackDepth,
                                     void *pvParameters,
                                     UBaseType_t uxPriority,
                                     TaskHandle_t *pxCreatedTask) {
    (void)pxTaskCode;
    (void)pcName;
    (void)usStackDepth;
    (void)pvParameters;
    (void)uxPriority;
    if (pxCreatedTask != NULL) {
        *pxCreatedTask = (TaskHandle_t)1;
    }
    return pdPASS;
}

static inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask) {
    (void)xTask;
    return 512u;   /* simulated minimum free stack, in words */
}

/* Yield request when a FromISR call woke a higher-priority task. The ISR
 * example must call this before returning so the scheduler can switch. */
#define portYIELD_FROM_ISR(x) do { if ((x) != pdFALSE) { g_stub.yield_count++; } } while (0)

#endif /* FREERTOS_STUBS_H */
