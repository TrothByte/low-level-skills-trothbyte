/*
 * TARGET-ONLY SKETCH — NOT compiled on this host (kernel headers absent).
 * Illustrates the CORRECT NAPI poll discipline for review. To build on a
 * Linux host with kernel headers for the running kernel:
 *
 *   make -C /lib/modules/$(uname -r)/build M=$PWD modules
 *
 * Key points this sketch demonstrates:
 *   - IRQ handler: minimal; napi_schedule() + mask the queue IRQ.
 *   - poll(): processes up to `budget` packets via napi_gro_receive().
 *   - napi_complete_done() called ONLY when work_done < budget; the return
 *     value controls re-enabling the queue IRQ.
 *   - Returning the full budget (work_done == budget) leaves the instance
 *     scheduled: no napi_complete, no IRQ re-enable.
 */
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/netpoll.h>

#define MYNIC_RING_SIZE 512

struct mynic {
    struct net_device *netdev;
    struct napi_struct napi;
    void __iomem *regs;
    /* receive ring state */
    unsigned int rx_head, rx_tail;
    struct sk_buff *rx_skb[MYNIC_RING_SIZE];
    unsigned int irq_mask_bit;
};

static inline struct mynic *mynic_from_napi(struct napi_struct *napi)
{
    return container_of(napi, struct mynic, napi);
}

static void mynic_disable_queue_irq(struct mynic *nic)
{
    /* clear the queue's interrupt-enable bit in the NIC's registers */
    iowrite32(0, nic->regs + nic->irq_mask_bit);
}

static void mynic_enable_queue_irq(struct mynic *nic)
{
    iowrite32(1, nic->regs + nic->irq_mask_bit);
}

static int mynic_poll(struct napi_struct *napi, int budget)
{
    struct mynic *nic = mynic_from_napi(napi);
    int work_done = 0;

    while (work_done < budget) {
        struct sk_buff *skb;
        unsigned int idx;

        if (nic->rx_head == nic->rx_tail)
            break;                      /* ring drained */

        idx = nic->rx_tail % MYNIC_RING_SIZE;
        skb = nic->rx_skb[idx];
        nic->rx_skb[idx] = NULL;
        nic->rx_tail++;

        skb_record_rx_queue(skb, 0);
        napi_gro_receive(napi, skb);    /* GRO path: never netif_receive_skb */
        work_done++;
    }

    if (work_done < budget) {
        /* ring empty: complete, and re-enable the IRQ only if the kernel
         * says the instance may stop (true) */
        if (napi_complete_done(napi, work_done))
            mynic_enable_queue_irq(nic);
    }
    /* work_done == budget: return without napi_complete; the softirq
     * re-runs this poll, IRQ stays masked */

    return work_done;
}

static irqreturn_t mynic_irq_handler(int irq, void *data)
{
    struct mynic *nic = data;

    /* minimal: schedule and mask the queue IRQ; all packet work happens
     * in mynic_poll() under NET_RX_SOFTIRQ, never here */
    mynic_disable_queue_irq(nic);
    if (napi_schedule_prep(&nic->napi))
        __napi_schedule(&nic->napi);
    return IRQ_HANDLED;
}

static int mynic_open(struct net_device *netdev)
{
    struct mynic *nic = netdev_priv(netdev);
    napi_enable(&nic->napi);
    mynic_enable_queue_irq(nic);
    return 0;
}

static int mynic_close(struct net_device *netdev)
{
    struct mynic *nic = netdev_priv(netdev);
    /* teardown order: stop IRQ -> napi_disable -> netif_napi_del */
    mynic_disable_queue_irq(nic);
    synchronize_irq(netdev->irq);
    napi_disable(&nic->napi);
    netif_napi_del(&nic->napi);
    return 0;
}

static int mynic_probe(struct net_device *netdev)
{
    struct mynic *nic = netdev_priv(netdev);
    /* weight = budget for this queue's poll round */
    netif_napi_add_weight(netdev, &nic->napi, mynic_poll, 64);
    return 0;
}
