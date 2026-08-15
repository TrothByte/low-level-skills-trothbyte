// intentionally incorrect — BAD example: posting a WR before the QP reaches RTS.
//
// ibv_post_send is only valid when the QP is in RTS (for SEND/RDMA WRs).
// Right after ibv_create_qp the QP is RESET; posting sends here is either an
// error from ibv_post_send or undefined. The full sequence RESET->INIT->RTR->
// RTS (with the per-transition attributes) is mandatory.
//
// Compare: examples/good/good_qp_setup.c

#include <infiniband/verbs.h>
#include <string.h>

void bad_post_before_rts(struct ibv_pd *pd, struct ibv_qp *qp,
                         struct ibv_qp_init_attr *init_attr)
{
    struct ibv_qp_attr attr;
    struct ibv_sge sge;
    struct ibv_send_wr wr, *bad_wr = NULL;

    /* BUG: create QP then post immediately — no RESET->INIT->RTR->RTS. */
    qp = ibv_create_qp(pd, init_attr);

    memset(&sge, 0, sizeof sge);
    sge.addr = 0; sge.length = 64; sge.lkey = 0;
    memset(&wr, 0, sizeof wr);
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.sg_list = &sge; wr.num_sge = 1;

    (void)ibv_post_send(qp, &wr, &bad_wr);   /* QP still RESET -> error */

    /* Missing: modify_qp to INIT, then RTR (with peer QPN + address),
       then RTS. Compare good_qp_setup.c for the complete sequence. */
    (void)attr;
}
