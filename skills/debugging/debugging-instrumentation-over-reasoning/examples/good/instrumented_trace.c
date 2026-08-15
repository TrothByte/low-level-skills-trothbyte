#include <stdio.h>

#define BUF_CAP 16
#define NSLOTS 16
#define NITER 20

struct area {
    char buf[BUF_CAP];
    int slots[NSLOTS];
};

static struct area A;
static int total_written;

static void copy_in(int src_len, FILE *log)
{
    int i;
    int before = A.slots[0];

    fprintf(log, "ENTRY copy_in len=%d total=%d\n", src_len, total_written);
#ifndef NOBUG
    if (total_written >= 11)
        src_len += 4;
#endif
    for (i = 0; i < src_len; i++)
        A.buf[i] = (char)('a' + (i % 26));
    total_written++;
    fprintf(log, "AFTER copy_in len=%d total=%d slots[0]=0x%08x was=0x%08x\n",
            src_len, total_written, A.slots[0], before);
    fflush(log);
}

int main(void)
{
    int k;
    int corrupted = -1;
    FILE *log = fopen("instrumentation.log", "a");
    if (log == NULL)
        return 1;

    fprintf(log, "RUN begin\n");
    for (k = 0; k < NITER; k++) {
        int len = (k % 2) ? 14 : 8;
        copy_in(len, log);
        if (A.slots[0] != 0) {
            corrupted = k;
            break;
        }
    }
    if (corrupted >= 0)
        fprintf(log, "CORRUPTION DETECTED at iteration %d total=%d slots[0]=0x%08x\n",
                corrupted, total_written, A.slots[0]);
    else
        fprintf(log, "no corruption observed\n");
    fflush(log);
    fclose(log);

    printf("result: %s\n", corrupted >= 0 ? "corruption found" : "no corruption");
    return 0;
}
