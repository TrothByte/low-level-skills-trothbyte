/* GOOD: disciplined triage outcome.
 *
 * Root cause fixed in one place: ingest() rejects input that does not fit
 * the target field, so the overflow never happens. crash_site.c and
 * symptom_guard.c fail this exact run: they crash, or exit 0 with a wrong
 * total. This version exits 0 with total == 185.
 */
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
    if (n > sizeof a->name) {
        fprintf(stderr, "ingest: rejected %zu-byte payload (field is %zu bytes)\n",
                n, sizeof a->name);
        return;
    }
    memcpy(a->name, raw, n);
}

static void show(const struct Account *a)
{
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
    long total;
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
    total = settle(acc);
    printf("total: %ld\n", total);
    if (total != 185)
        return 1;

    free(acc);
    return 0;
}
