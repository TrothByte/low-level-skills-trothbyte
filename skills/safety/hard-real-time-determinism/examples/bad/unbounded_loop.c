/*
 * unbounded_loop.c - BANNED PATTERN: data-dependent and unbounded loops.
 *
 * A loop whose trip count depends on the input data has no static bound, so
 * its WCET is not provable: the worst case is "until the data ends", which
 * may be never. strlen over untrusted data is the same hazard in library
 * form. The bounded pattern (examples/good/bounded_task.c) bounds the trip
 * count by a constant and checks the terminator inside the loop.
 *
 * This fixture compiles and runs on the host; flagging is the point.
 * Flagged by rt_banned_patterns.py as unbounded loop / input scan.
 */

#include <stdio.h>
#include <string.h>

static int has_more_input(void);

static int consume_until_zero(const char *stream)
{
    int n = 0;
    while (*stream != '\0') {     /* BANNED: trip count depends on data */
        n++;
        stream++;
    }
    return n;
}

static int drain_loop(void)
{
    int total = 0;
    while (has_more_input()) {    /* BANNED: function-call condition, no bound */
        total++;
    }
    return total;
}

static int process_input(const char *data)
{
    int len = (int)strlen(data);  /* BANNED: input-dependent scan */
    return len;
}

static int has_more_input(void)
{
    return 0;
}

int main(void)
{
    printf("consumed=%d len=%d\n", consume_until_zero("hello"),
           process_input("hello"));
    (void)drain_loop();
    return 0;
}
