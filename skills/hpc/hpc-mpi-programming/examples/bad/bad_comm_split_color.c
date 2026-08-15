// intentionally incorrect — BAD example: wrong Comm_split color.
//
// MPI_Comm_split groups ranks by color; ranks with different colors end up in
// DIFFERENT new communicators. Here all ranks use color 0 (everyone in one
// communicator), while the intent was a 2-group split. The key is then
// meaningless for grouping. Also the code assumes the new communicator keeps
// the original rank numbers.

#include <mpi.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    int rank, size;
    MPI_Comm newcomm;
    int newrank = -1;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // BUG: color is always 0, so ALL ranks stay in one communicator instead of
    // the intended {0,1} and {2,3} groups.
    int color = 0;
    int key = rank;
    MPI_Comm_split(MPI_COMM_WORLD, color, key, &newcomm);
    MPI_Comm_rank(newcomm, &newrank);

    printf("rank %d -> newrank %d (intended two groups of %d)\n",
           rank, newrank, size / 2);

    MPI_Finalize();
    return 0;
}
