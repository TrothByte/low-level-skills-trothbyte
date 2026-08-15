// intentionally incorrect — BAD example: blocking Send/Send deadlock.
//
// Both ranks call MPI_Send to each other BEFORE posting MPI_Recv. With the
// rendezvous protocol (large message), both block forever: rank 0 waits for
// rank 1 to receive, rank 1 waits for rank 0 to receive. Small messages may be
// buffered and "work", which is why this bug survives casual testing.
//
// Compare: examples/good/good_nonblocking.c

#include <mpi.h>
#include <stdlib.h>

#define N (1 << 20)   /* large enough to defeat eager buffering */

int main(int argc, char **argv)
{
    int rank, size;
    int *a, *b;
    MPI_Status st;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    a = malloc(N * sizeof(int));
    b = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) a[i] = rank + i;

    if (rank == 0) {
        MPI_Send(a, N, MPI_INT, 1, 0, MPI_COMM_WORLD);   /* blocks */
        MPI_Recv(b, N, MPI_INT, 1, 0, MPI_COMM_WORLD, &st);
    } else if (rank == 1) {
        MPI_Send(a, N, MPI_INT, 0, 0, MPI_COMM_WORLD);   /* blocks */
        MPI_Recv(b, N, MPI_INT, 0, 0, MPI_COMM_WORLD, &st);
    }
    /* DEADLOCK: each Send waits for a Recv that is never posted. */

    free(a);
    free(b);
    MPI_Finalize();
    return 0;
}
