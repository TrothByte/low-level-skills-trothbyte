/* Correct harness: coverage gate — every branch of the target is exercised,
   and the harness only reports PASS when each branch was reached. */
#include <assert.h>
#include <stdio.h>

static int clamp_low = 0;
static int clamp_high = 0;
static int passthrough = 0;

int bounded_value(int x)
{
    if (x < 0) {
        clamp_low++;
        return 0;
    }
    if (x > 100) {
        clamp_high++;
        return 100;
    }
    passthrough++;
    return x;
}

int main(void)
{
    assert(bounded_value(-1) == 0);
    assert(bounded_value(1000) == 100);
    assert(bounded_value(55) == 55);

    /* Coverage gate: all three regions of bounded_value were executed.
       If the target gained a branch the harness does not know about,
       this gate does not catch it — add a call for every region. */
    assert(clamp_low == 1 && "low clamp branch never executed");
    assert(clamp_high == 1 && "high clamp branch never executed");
    assert(passthrough == 1 && "passthrough branch never executed");

    printf("harness PASS: full branch coverage achieved\n");
    return 0;
}
