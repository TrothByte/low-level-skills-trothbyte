// GOOD: non-blocking ping-pong with Irecv posted before Isend.
//
// Both ranks post Irecv before Isend, then wait for both. This is deadlock-free
// for ANY message size and any buffering behavior: the receive is always
// available when the peer's send arrives.

#include <mpi.h>
#include <stdlib.h>

#define N (1 << 20)

int main(int argc, char **argv)
{
    int rank;
    int *a, *b;
    MPI_Request reqs[2];
    MPI_Status stats[2];

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    a = malloc(N * sizeof(int));
    b = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) a[i] = rank + i;

    int peer = 1 - rank;
    MPI_Irecv(b, N, MPI_INT, peer, 0, MPI_COMM_WORLD, &reqs[0]);
    MPI_Isend(a, N, MPI_INT, peer, 0, MPI_COMM_WORLD, &reqs[1]);
    MPI_Waitall(2, reqs, stats);       /* both complete; buffers now reusable */

    free(a);
    free(b);
    MPI_Finalize();
    return 0;
}
