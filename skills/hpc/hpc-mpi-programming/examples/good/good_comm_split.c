// GOOD: MPI_Comm_split into two groups with a key ordering, and a matching
// collective within the new communicator.
//
// color = rank/2 groups {0,1} and {2,3}; key = rank%2 orders within each group.
// Then a collective (Bcast) is called on the NEW communicator with the same
// signature on every rank of that communicator.

#include <mpi.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    int rank, newrank, size;
    MPI_Comm newcomm;
    int root = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int color = rank / 2;      /* groups {0,1} and {2,3} */
    int key = rank % 2;        /* order 0,1 within each color */
    MPI_Comm_split(MPI_COMM_WORLD, color, key, &newcomm);
    MPI_Comm_rank(newcomm, &newrank);

    int msg = (rank == 0 || rank == 2) ? 99 : 0;  /* per-group root value */
    MPI_Bcast(&msg, 1, MPI_INT, root, newcomm);   /* collective on newcomm */

    printf("rank %d -> newrank %d, bcast value %d\n", rank, newrank, msg);

    MPI_Comm_free(&newcomm);
    MPI_Finalize();
    return 0;
}
