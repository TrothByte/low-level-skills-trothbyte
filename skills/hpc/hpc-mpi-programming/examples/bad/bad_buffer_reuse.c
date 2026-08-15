// intentionally incorrect — BAD example: buffer reused after Irecv before Wait.
//
// MPI_Irecv returns immediately; the buffer belongs to the in-flight receive
// until the request completes via Wait/Test. Writing into `buf` before
// MPI_Wait races with the receive and the received data is lost/corrupted.

#include <mpi.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int rank;
    char buf[64];
    MPI_Request req;
    MPI_Status st;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        strcpy(buf, "ping");
        MPI_Send(buf, 5, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
    } else {
        MPI_Irecv(buf, 64, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &req);
        // BUG: overwrite the receive buffer before the receive completes.
        memset(buf, 'X', sizeof(buf));
        MPI_Wait(&req, &st);   // buf now contains Xs, not "ping"
    }

    MPI_Finalize();
    return 0;
}
