// intentionally incorrect: invented verbs API — ibv_create_qpair and
// ibv_post_write do not exist in infiniband/verbs.h. Also claims that
// RDMA_WRITE needs no MR at all. Real API: ibv_create_qp + ibv_post_send
// with IBV_WR_RDMA_WRITE and a registered MR (remote_write access).
#include <infiniband/verbs.h>

void fake(void)
{
    struct ibv_qpair *q = ibv_create_qpair(0, NULL);
    ibv_post_write(q, NULL, 0);   /* no such function */
}
