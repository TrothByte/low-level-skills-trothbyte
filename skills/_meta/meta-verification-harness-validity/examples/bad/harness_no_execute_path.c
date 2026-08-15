// intentionally incorrect: verification path is never executed by default
#include <stdio.h>

int compute_checksum(const unsigned char *data, int len)
{
    int sum = 0;
    int i;
    for (i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum & 0xFFFF;
}

int main(int argc, char **argv)
{
    /* The self-check branch requires an environment variable that the CI
       invocation does not set, so the assertion block never runs and a
       broken checksum still "passes". */
    if (argv[1] != 0 && argv[1][0] == '1') {
        unsigned char buf[3] = {1, 2, 3};
        if (compute_checksum(buf, 3) != 6) {
            return 1;
        }
    }
    printf("checksum harness reported PASS\n");
    return 0;
}
