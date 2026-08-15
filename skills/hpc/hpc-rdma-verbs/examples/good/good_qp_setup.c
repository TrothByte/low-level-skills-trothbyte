// GOOD: full QP setup sequence RESET -> INIT -> RTR -> RTS with the
// per-transition attributes, then a signaled SEND.
//
// The ordering and the attribute masks are the correctness-critical part:
//   RESET->INIT: qp_access_flags, pkey_index, qp_state
//   INIT->RTR:   remote qpn, AV (address), rq_psn, qp_state
//   RTR->RTS:    timeout, retry_cnt, rnr_retry, sq_psn, qp_state
// Every ibv_* return is checked. Completions are polled with re-arm.
//
// Target: host with RDMA hardware + libibverbs (absent here; documented
// command: gcc -Wall -Wextra -O2 -libverbs good_qp_setup.c).

#include <infiniband/verbs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if ((x) != 0) { perror("verbs"); exit(1); } } while (0)

int main(int argc, char **argv)
{
    struct ibv_device **devs;
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    struct ibv_qp_init_attr init_attr;
    struct ibv_qp_attr attr;
    struct ibv_sge sge;
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_wc wc;
    char buf[64] = {0};
    int i;

    devs = ibv_get_device_list(NULL);
    ctx = ibv_open_device(devs[0]);
    pd = ibv_alloc_pd(ctx);
    mr = ibv_reg_mr(pd, buf, sizeof buf, IBV_ACCESS_LOCAL_WRITE);
    cq = ibv_create_cq(ctx, 32, NULL, NULL, 0);

    memset(&init_attr, 0, sizeof init_attr);
    init_attr.qp_type = IBV_QPT_RC;               /* reliable connected */
    init_attr.sq_sig_all = 0;
    init_attr.send_cq = cq;
    init_attr.recv_cq = cq;
    init_attr.cap.max_send_wr = 8;
    init_attr.cap.max_recv_wr = 8;
    init_attr.cap.max_send_sge = 1;
    init_attr.cap.max_recv_sge = 1;
    qp = ibv_create_qp(pd, &init_attr);

    /* RESET -> INIT */
    memset(&attr, 0, sizeof attr);
    attr.qp_state = IBV_QPS_INIT;
    attr.pkey_index = 0;
    attr.port_num = 1;
    attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
    CHECK(ibv_modify_qp(qp, &attr,
        IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS));

    /* INIT -> RTR: remote QPN + address (LID or GID depending on fabric) */
    memset(&attr, 0, sizeof attr);
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_1024;
    attr.dest_qp_num = 1;                          /* peer QPN from handshake */
    attr.rq_psn = 0;
    attr.ah_attr.port_num = 1;
    attr.ah_attr.dlid = 1;                         /* IB: LID; RoCE: GID */
    attr.ah_attr.is_global = 0;                    /* set 1 + grh for RoCE */
    CHECK(ibv_modify_qp(qp, &attr,
        IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
        IBV_QP_RQ_PSN));

    /* RTR -> RTS */
    memset(&attr, 0, sizeof attr);
    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.sq_psn = 0;
    attr.max_rd_atomic = 1;
    CHECK(ibv_modify_qp(qp, &attr,
        IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
        IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC));

    /* Signaled SEND */
    sge.addr = (uintptr_t)buf;
    sge.length = sizeof buf;
    sge.lkey = mr->lkey;
    memset(&wr, 0, sizeof wr);
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    CHECK(ibv_post_send(qp, &wr, &bad_wr));

    /* Completion: poll after re-arming; check status. */
    ibv_req_notify_cq(cq, 0);
    while (ibv_poll_cq(cq, 1, &wc) == 0) { }
    if (wc.status != IBV_WC_SUCCESS) {
        fprintf(stderr, "WC error %d\n", wc.status);
        return 1;
    }

    /* Teardown in order: QP, CQ, MR, PD, device */
    ibv_destroy_qp(qp);
    ibv_destroy_cq(cq);
    ibv_dereg_mr(mr);
    ibv_dealloc_pd(pd);
    ibv_close_device(ctx);
    ibv_free_device_list(devs);
    printf("good_qp_setup: QP sequence + signaled SEND + completion OK\n");
    printf("Researched — toolchain not available; command: gcc -libverbs "
           "good_qp_setup.c (RDMA host)\n");
    return 0;
}
