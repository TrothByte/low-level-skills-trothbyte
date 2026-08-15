// intentionally incorrect — BAD example: rank-dependent collective count.
//
// MPI_Allreduce must be called by all ranks with the same signature. Here the
// count differs per rank. The standard permits ranks to finish at different
// times, but the collective MUST match: mismatched count/type on one rank makes
// the whole collective undefined — typically a hang or buffer corruption.

#include <mpi.h>

int main(int argc, char **argv)
{
    int rank, n = 4;
    float sum = 0.0f;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // BUG: count depends on rank -> ranks call Allreduce with different
    // signatures, so the collective never matches across ranks.
    int count = (rank == 0) ? n : 1;
    MPI_Allreduce(MPI_IN_PLACE, &sum, count, MPI_FLOAT, MPI_SUM,
                  MPI_COMM_WORLD);

    MPI_Finalize();
    return 0;
}
