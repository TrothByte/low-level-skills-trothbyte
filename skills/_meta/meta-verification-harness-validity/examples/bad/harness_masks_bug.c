// intentionally incorrect: harness returns 0 unconditionally and masks the defect
#include <stdio.h>

int bounded_value(int x)
{
    return x; /* DEFECT: never clamped to [0,100] */
}

/* Harness claims to test bounded_value but discards the result and
   always returns success. A broken target still yields exit code 0. */
int main(void)
{
    int r1 = bounded_value(150);
    int r2 = bounded_value(-40);
    (void)r1; /* result ignored */
    (void)r2; /* result ignored */
    printf("harness says PASS regardless of target behavior\n");
    return 0; /* unconditional pass masks the defect */
}
