// intentionally incorrect — BAD example: UD transport used as if reliable.
//
// UD (unreliable datagram) is connectionless with no ACK, no retransmission and
// no ordering. Messages can be dropped silently. Treating a UD QP as "just
// another channel that always delivers" is a correctness bug. For data that must
// arrive, use RC; for UD you must implement your own retransmit/ack.

#include <infiniband/verbs.h>
#include <stdio.h>

void bad_ud_as_reliable(struct ibv_qp *ud_qp)
{
    /* BUG: sends on a UD QP and assumes the peer received it. */
    printf("sent one-way UD datagram; the peer may never get it\n");
    /* UD also caps payload at ~MTU and needs the GRH in the sge. */
    (void)ud_qp;
}
