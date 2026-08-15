// GOOD: RDMA_WRITE with a valid remote rkey, plus a poll-with-re-arm
// completion loop and MR kept alive until the WR completes.
//
// The rkey comes from the remote side's ibv_reg_mr and is exchanged during
// the handshake. The MR is only deregistered after the completion is polled.

#include <infiniband/verbs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int good_rdma_write(struct ibv_qp *qp, struct ibv_mr *local_mr,
                    uint64_t remote_addr, uint32_t remote_rkey)
{
    struct ibv_sge sge;
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_wc wc;
    struct ibv_cq *cq = qp->send_cq;

    memset(&sge, 0, sizeof sge);
    sge.addr = 0;                       /* local buffer in the MR */
    sge.length = 64;
    sge.lkey = local_mr->lkey;

    memset(&wr, 0, sizeof wr);
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.wr.rdma.remote_addr = remote_addr;
    wr.wr.rdma.rkey = remote_rkey;      /* REQUIRED for remote access */

    if (ibv_post_send(qp, &wr, &bad_wr) != 0)
        return -1;

    /* Wait for the WR completion (poll with re-arm); check status. */
    ibv_req_notify_cq(cq, 0);
    while (ibv_poll_cq(cq, 1, &wc) == 0) { }
    if (wc.status != IBV_WC_SUCCESS)
        return -2;

    /* Now (and only now) the local MR/buffer may be reused/deregistered. */
    return 0;
}
