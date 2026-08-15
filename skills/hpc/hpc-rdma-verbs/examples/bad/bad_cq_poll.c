// intentionally incorrect — BAD example: completion polling without re-arming.
//
// ibv_poll_cq returns what is available; ibv_req_notify_cq arms the CQ event
// for the NEXT completion. The classic loop is: arm -> wait -> poll -> drain ->
// re-arm. This code polls once and treats "nothing ready" as "not complete",
// then never re-arms, so a completion that arrives just after the poll is
// missed forever (in the event-driven model).

#include <infiniband/verbs.h>
#include <stdio.h>

int bad_poll_once(struct ibv_cq *cq)
{
    struct ibv_wc wc;
    /* BUG: no ibv_req_notify_cq, no wait, no drain loop. */
    int n = ibv_poll_cq(cq, 1, &wc);
    if (n == 0) {
        printf("no completion ready (BUG: completion may arrive later)\n");
        return 0;                      /* not "never completes"! */
    }
    if (wc.status != IBV_WC_SUCCESS) return -1;
    return (int)wc.wr_id;
}
