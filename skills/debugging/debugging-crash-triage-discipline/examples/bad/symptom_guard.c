// intentionally incorrect
// The "false fixed" verdict: the previous crash (see crash_site.c) is
// silenced by guarding the symptom (skip accounts with a NULL owner) instead
// of removing the overflow. The program now exits 0, so a naive check "it
// runs, therefore it is fixed" passes. But account[2]/account[3] are still
// corrupted by ingest(): balances were overwritten with 0x4141414141414141,
// so settle() reports a wrong total. The root cause remained; the symptom
// merely stopped crashing.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 4

struct Account {
    char name[8];
    long balance;
    char *owner;
};

static char *OWNERS[N];

static void init_owners(void)
{
    OWNERS[0] = "alice";
    OWNERS[1] = "bob";
    OWNERS[2] = "carol";
    OWNERS[3] = "dave";
}

static void seed(struct Account *a, int i, long bal)
{
    a->balance = bal;
    a->owner = OWNERS[i % N];
}

static void ingest(struct Account *a, const unsigned char *raw, size_t n)
{
    memcpy(a->name, raw, n);
}

static void show(struct Account *a)
{
    if (a->owner == NULL)
        return;
    printf("name: %s\n", a->name);
    printf("owner: %s\n", a->owner);
}

static long settle(const struct Account *acc)
{
    long total = 0;
    int i;

    for (i = 0; i < N; i++)
        total += acc[i].balance;
    return total;
}

int main(void)
{
    unsigned char blob[48];
    struct Account *acc = calloc(N, sizeof *acc);

    init_owners();
    seed(&acc[0], 0, 100);
    seed(&acc[1], 1, 50);
    seed(&acc[2], 2, 25);
    seed(&acc[3], 3, 10);

    memset(blob, 'A', sizeof blob);
    memset(blob + 40, 0, 8);

    ingest(&acc[2], blob, sizeof blob);

    show(&acc[0]);
    show(&acc[3]);
    printf("total: %ld\n", settle(acc));

    free(acc);
    return 0;
}
