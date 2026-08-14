/* cortex_m_stubs.h — host-compilable model of the Cortex-M interrupt machinery
 * used by the skill examples. It encodes four facts the skill teaches:
 * (1) NVIC priority values fit in __NVIC_PRIO_BITS bits and a lower number
 * means higher urgency; (2) priorities below OS_PRIO_FLOOR are reserved for
 * the OS and must not go to device IRQs; (3) __disable_irq()/__enable_irq()
 * (PRIMASK) form a critical section that delays pending interrupts; (4) work
 * charged inside an ISR is checked against a budget. */
#ifndef CORTEX_M_STUBS_H
#define CORTEX_M_STUBS_H

#include <pthread.h>
#include <stdlib.h>

#if defined(__GNUC__)
#define STUB_UNUSED __attribute__((unused))
#else
#define STUB_UNUSED
#endif

#define __NVIC_PRIO_BITS 4u
#define NVIC_PRIO_MAX ((1u << __NVIC_PRIO_BITS) - 1u)

/* Device IRQs must be at priority >= OS_PRIO_FLOOR. */
#define OS_PRIO_FLOOR 2u

#define STUB_ISR_BUDGET 8u

typedef enum {
    IRQ_UART = 0,
    IRQ_TIMER,
    IRQ_SPI,
    IRQ_COUNT
} IRQn_Type;

typedef enum {
    STUB_OK = 0,
    STUB_FAIL_PRIO_RANGE,
    STUB_FAIL_PRIO_RESERVED,
    STUB_FAIL_ENABLE_UNCONFIGURED,
    STUB_FAIL_ISR_OVER_BUDGET,
    STUB_FAIL_UNBALANCED_CRIT,
    STUB_FAIL_RECURSIVE_CRIT
} stub_fail_kind_t;

typedef struct {
    unsigned priority[IRQ_COUNT];
    unsigned priority_set[IRQ_COUNT];
    unsigned enabled[IRQ_COUNT];
    unsigned pending[IRQ_COUNT];
    unsigned irq_depth;
    unsigned work_units;
    unsigned primask_locked;
    int fail_count;
    stub_fail_kind_t fail;
} stub_nvic_t;

static stub_nvic_t g_nvic;

/* PRIMASK model: while this mutex is held by a critical section, an arriving
 * ISR is pended (blocks) and runs after __enable_irq() releases it. */
static STUB_UNUSED pthread_mutex_t g_primask = PTHREAD_MUTEX_INITIALIZER;

static inline void __disable_irq(void) {
    if (g_nvic.primask_locked) {
        g_nvic.fail = STUB_FAIL_RECURSIVE_CRIT;
        g_nvic.fail_count++;
        return;
    }
    pthread_mutex_lock(&g_primask);
    g_nvic.primask_locked = 1u;
}

static inline void __enable_irq(void) {
    if (!g_nvic.primask_locked) {
        g_nvic.fail = STUB_FAIL_UNBALANCED_CRIT;
        g_nvic.fail_count++;
        return;
    }
    g_nvic.primask_locked = 0u;
    pthread_mutex_unlock(&g_primask);
}

static inline unsigned nvic_get_priority(IRQn_Type irq) {
    return g_nvic.priority[irq];
}

static inline void nvic_set_priority(IRQn_Type irq, unsigned prio) {
    if (prio > NVIC_PRIO_MAX) {
        g_nvic.fail = STUB_FAIL_PRIO_RANGE;
        g_nvic.fail_count++;
        return;
    }
    if (prio < OS_PRIO_FLOOR) {
        g_nvic.fail = STUB_FAIL_PRIO_RESERVED;
        g_nvic.fail_count++;
        return;
    }
    g_nvic.priority[irq] = prio;
    g_nvic.priority_set[irq] = 1u;
}

static inline void nvic_enable_irq(IRQn_Type irq) {
    if (!g_nvic.priority_set[irq]) {
        g_nvic.fail = STUB_FAIL_ENABLE_UNCONFIGURED;
        g_nvic.fail_count++;
        return;
    }
    g_nvic.enabled[irq] = 1u;
}

static inline void nvic_disable_irq(IRQn_Type irq) {
    g_nvic.enabled[irq] = 0u;
}

static inline void nvic_set_pending(IRQn_Type irq) { g_nvic.pending[irq] = 1u; }
static inline void nvic_clear_pending(IRQn_Type irq) { g_nvic.pending[irq] = 0u; }
static inline unsigned nvic_get_pending(IRQn_Type irq) { return g_nvic.pending[irq]; }

static inline void stub_enter_isr(void) { g_nvic.irq_depth++; }
static inline void stub_exit_isr(void) {
    if (g_nvic.irq_depth > 0u) {
        g_nvic.irq_depth--;
    }
}
static inline unsigned stub_isr_active(void) { return g_nvic.irq_depth > 0u; }

/* Charge ISR work against the budget; a busy-wait in an ISR trips it. */
static inline void stub_isr_work(unsigned units) {
    if (stub_isr_active()) {
        g_nvic.work_units += units;
        if (g_nvic.work_units > STUB_ISR_BUDGET) {
            g_nvic.fail = STUB_FAIL_ISR_OVER_BUDGET;
            g_nvic.fail_count++;
        }
    }
}

static inline int stub_has_failed(void) { return g_nvic.fail_count > 0; }
static inline stub_fail_kind_t stub_failure(void) { return g_nvic.fail; }

#endif /* CORTEX_M_STUBS_H */
