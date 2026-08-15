// intentionally incorrect
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

static void copy_in(int src_len)
{
    int i;
    if (total_written >= 11)
        src_len += 4;
    for (i = 0; i < src_len; i++)
        A.buf[i] = (char)('a' + (i % 26));
    total_written++;
}

int main(void)
{
    int k;
    for (k = 0; k < NITER; k++) {
        copy_in((k % 2) ? 14 : 8);
        if (A.slots[0] != 0)
            A.slots[0] = 0;
    }
    if (A.slots[0] == 0)
        printf("PASS: slots[0]=0x%08x after %d writes\n", A.slots[0], total_written);
    else
        printf("FAIL: slots[0]=0x%08x\n", A.slots[0]);
    return A.slots[0] == 0 ? 0 : 1;
}
