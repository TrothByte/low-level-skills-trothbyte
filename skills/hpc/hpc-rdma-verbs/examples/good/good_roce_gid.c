// GOOD: transport and addressing chosen by the fabric — RoCE vs InfiniBand.
//
// InfiniBand addresses the remote QP by a 16-bit LID; RoCE uses GID/IP (RoCEv2
// UDP+IP). Hard-coding a LID breaks on RoCE. Query the port's link layer and set
// ah_attr.is_global + grh accordingly.

#include <infiniband/verbs.h>
#include <stdio.h>

int setup_ah_attr(struct ibv_context *ctx, struct ibv_ah_attr *ah,
                  uint8_t port_num, uint16_t lid)
{
    struct ibv_port_attr pa;

    if (ibv_query_port(ctx, port_num, &pa) != 0) return -1;

    if (pa.link_layer == IBV_LINK_LAYER_INFINIBAND) {
        ah->dlid = lid;                 /* LID-based addressing */
        ah->is_global = 0;
    } else if (pa.link_layer == IBV_LINK_LAYER_ETHERNET) {
        /* RoCEv2: GID-based; is_global=1, grh.sgid_index + dgid from
           ibv_query_gid. (RoCEv1 would use the embedded GRH as well.) */
        ah->is_global = 1;
        ah->grh.sgid_index = 0;
        /* dgid filled from the handshake GID — never the raw LID */
    } else {
        return -2;
    }
    return 0;
}
