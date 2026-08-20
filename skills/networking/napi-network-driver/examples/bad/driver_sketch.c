/*
 * TARGET-ONLY SKETCH — NOT compiled on this host (kernel headers absent).
 * Demonstrates the BUGGY NAPI patterns that reviewers must catch.
 * Do NOT build: this file is a fixture for the static checker
 * (`examples/tools/napi_check.py`) and for manual review.
 *
 * Bugs intentionally present:
 *   1. poll() calls napi_complete() unconditionally (also at budget) and
 *      re-enables the IRQ while the ring is still full.
 *   2. poll() uses netif_receive_skb() instead of napi_gro_receive().
 *   3. poll() takes mutex_lock() (blocking, forbidden in softirq context).
 *   4. IRQ handler processes packets itself (heavy work in IRQ context).
 *   5. IRQ handler schedules NAPI without masking the queue IRQ.
 */
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/mutex.h>

#define MYNIC_RING_SIZE 512

static struct mutex mynic_lock;

static int mynic_poll_buggy(struct napi_struct *napi, int budget)
{
    int work_done = 0;
    int idx = 0;

    /* BUG 3: mutex_lock() in poll — sleeps in softirq context */
    mutex_lock(&mynic_lock);

    while (work_done < budget) {
        struct sk_buff *skb = /* ...dequeue from ring... */ NULL;
        if (!skb)
            break;
        /* BUG 2: bypasses GRO; kills coalescing for GRO-enabled paths */
        netif_receive_skb(skb);
        work_done++;
    }

    /* BUG 1: unconditional complete + IRQ re-enable even when
     * work_done == budget and the ring still has packets */
    napi_complete(napi);
    /* ...enable queue IRQ here... */

    mutex_unlock(&mynic_lock);
    return work_done;
}

static irqreturn_t mynic_irq_buggy(int irq, void *data)
{
    /* BUG 4: heavy work in the IRQ handler — processing packets here
     * defeats NAPI entirely (no batching, long IRQ-masked time) */
    while (/* ring not empty */ 0) {
        struct sk_buff *skb = /* ...dequeue... */ NULL;
        netif_receive_skb(skb);       /* and/or protocol hooks */
    }

    /* BUG 5: schedules without masking the queue IRQ — a re-entrant IRQ
     * arrives while the instance is already scheduled */
    napi_schedule(napi);

    return IRQ_HANDLED;
}
