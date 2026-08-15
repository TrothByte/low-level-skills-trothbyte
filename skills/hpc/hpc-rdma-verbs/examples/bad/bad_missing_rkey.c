// intentionally incorrect — BAD example: RDMA_WRITE without a remote rkey.
//
// RDMA write/read/atomic bypass the remote CPU and are authorized by the
// remote MR's rkey. Here the WR sets remote_addr but leaves rkey = 0; the HCA
// rejects it (remote access error; completion with error, QP -> ERR). The rkey
// must come from the remote side's ibv_reg_mr during the handshake.

#include <infiniband/verbs.h>
#include <string.h>

void bad_missing_rkey(struct ibv_qp *qp, uint64_t remote_addr)
{
    struct ibv_sge sge;
    struct ibv_send_wr wr, *bad_wr = NULL;

    memset(&sge, 0, sizeof sge);
    sge.addr = 0; sge.length = 64; sge.lkey = 0;

    memset(&wr, 0, sizeof wr);
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.sg_list = &sge; wr.num_sge = 1;
    wr.wr.rdma.remote_addr = remote_addr;
    /* BUG: wr.wr.rdma.rkey never set -> remote access error. */

    (void)ibv_post_send(qp, &wr, &bad_wr);
}
