// GOOD: MPI-IO with byte offsets and collective participation.
//
// Each rank writes its portion at byte offset rank*count*sizeof(int). Offsets
// are BYTES, not elements — the classic bug is using rank*count bytes, which
// makes consecutive ranks' regions overlap.

#include <mpi.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    int rank, size;
    int count = 4;
    int *buf;
    MPI_File fh;
    MPI_Status st;
    MPI_Offset offset;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    buf = (int *)malloc(count * sizeof(int));
    for (int i = 0; i < count; i++) buf[i] = rank * 100 + i;

    MPI_File_open(MPI_COMM_WORLD, "out.bin", MPI_MODE_CREATE | MPI_MODE_WRONLY,
                  MPI_INFO_NULL, &fh);

    // Byte offset: rank * count * sizeof(int).
    offset = (MPI_Offset)rank * (MPI_Offset)count * (MPI_Offset)sizeof(int);
    MPI_File_write_at_all(fh, offset, buf, count, MPI_INT, &st);

    MPI_File_close(&fh);
    free(buf);
    MPI_Finalize();
    return 0;
}
