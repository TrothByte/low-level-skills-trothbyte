/* Correct harness for a genuinely correct target — must NOT be flagged.
   This is the false-positive guard for the skill: a real passing harness
   (asserts that can fail, coverage of all branches) is valid. */
#include <assert.h>
#include <stdio.h>

static int checked_add(int a, int b, int *out)
{
    long long sum = (long long)a + b;
    if (sum > 2147483647LL || sum < -2147483648LL) {
        return -1; /* overflow */
    }
    *out = (int)sum;
    return 0;
}

int main(void)
{
    int r;

    assert(checked_add(1, 2, &r) == 0 && r == 3);
    assert(checked_add(2147483647, 1, &r) == -1); /* overflow path */
    assert(checked_add(-2147483648, -1, &r) == -1);
    printf("harness PASS: checked_add holds for edge cases\n");
    return 0;
}
