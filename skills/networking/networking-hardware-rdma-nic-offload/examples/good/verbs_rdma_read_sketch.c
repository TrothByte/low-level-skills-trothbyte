/* Correct verbs skeleton for a one-sided RDMA_READ: PD -> MR (REMOTE_READ) ->
 * QP -> RTR/RTS -> post_send RDMA_READ -> poll CQ. Uses the real API surface
 * (infiniband/verbs.h). Compiles on Linux with -libverbs; NOT compiled on this
 * host (no libibverbs). Host-side stand-in: examples/check_verbs_trace.py.
 */
#include <infiniband/verbs.h>
#include <stdint.h>
#include <stdlib.h>

static char *buf;
static struct ibv_pd *pd;
static struct ibv_mr *mr;
static struct ibv_qp *qp;
static struct ibv_cq *cq;

int rdma_read_setup(struct ibv_context *ctx)
{
    struct ibv_qp_init_attr qia = {0};
    struct ibv_qp_attr qa;
    struct ibv_sge sge;
    struct ibv_send_wr wr = {0}, *bad;
    struct ibv_recv_wr rwr = {0}, *rbad;
    struct ibv_wc wc[1];

    buf = aligned_alloc(4096, 4096);
    if (!buf) return -1;

    pd = ibv_alloc_pd(ctx);
    if (!pd) return -1;

    mr = ibv_reg_mr(pd, buf, 4096, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ);
    if (!mr) return -1;

    cq = ibv_create_cq(ctx, 4, NULL, NULL, 0);
    if (!cq) return -1;

    qia.send_cq = cq;
    qia.recv_cq = cq;
    qia.qp_type = IBV_QPT_RC;
    qia.cap.max_send_wr = 4;
    qia.cap.max_recv_wr = 4;
    qia.cap.max_send_sge = 1;
    qia.cap.max_recv_sge = 1;
    qp = ibv_create_qp(pd, &qia);
    if (!qp) return -1;

    qa.qp_state = IBV_QPS_INIT;
    qa.pkey_index = 0;
    qa.port_num = 1;
    qa.qp_access_flags = IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC;
    if (ibv_modify_qp(qp, &qa, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |
                                 IBV_QP_ACCESS_FLAGS))
        return -1;

    qa.qp_state = IBV_QPS_RTR;
    qa.path_mtu = IBV_MTU_4096;
    qa.dest_qp_num = qp->qp_num;
    qa.rq_psn = 0;
    qa.ah_attr.is_global = 0;
    qa.ah_attr.port_num = 1;
    if (ibv_modify_qp(qp, &qa, IBV_QP_STATE | IBV_QP_MTU | IBV_QP_DEST_QPN |
                                 IBV_QP_RQ_PSN | IBV_QP_AV))
        return -1;

    qa.qp_state = IBV_QPS_RTS;
    qa.sq_psn = 0;
    if (ibv_modify_qp(qp, &qa, IBV_QP_STATE | IBV_QP_SQ_PSN))
        return -1;

    sge.addr = (uintptr_t)buf;
    sge.length = 4096;
    sge.lkey = mr->lkey;
    rwr.sg_list = &sge;
    rwr.num_sge = 1;
    if (ibv_post_recv(qp, &rwr, &rbad)) return -1;

    wr.wr_id = 1;
    wr.opcode = IBV_WR_RDMA_READ;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.sg_list->lkey = mr->lkey;
    wr.rdma.remote_addr = (uintptr_t)buf;
    wr.rdma.rkey = mr->rkey;
    if (ibv_post_send(qp, &wr, &bad)) return -1;

    return ibv_poll_cq(cq, 1, wc) == 1 && wc[0].status == IBV_WC_SUCCESS ? 0 : -1;
}
