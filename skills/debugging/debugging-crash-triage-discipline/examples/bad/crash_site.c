// intentionally incorrect
// The crash site is a symptom, not the bug.
// ingest() writes 48 bytes into an 8-byte field. The write lands inside the
// same heap block (account[3]), so no allocator metadata is touched and no
// ASan-style trap fires. account[3].owner is zeroed by the overflow; show()
// then passes NULL to print_owner and crashes. The fault is reported in
// print_owner/strlen, far from the memcpy that caused it.
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

static void print_owner(const char *owner)
{
    size_t len = strlen(owner);
    printf("owner (%zu): %s\n", len, owner);
}

static void show(struct Account *a)
{
    printf("name: %s\n", a->name);
    print_owner(a->owner);
    fflush(stdout);
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

    free(acc);
    return 0;
}
