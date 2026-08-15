#include <stdio.h>
#include <zlib.h>

int main(void)
{
    char out[32];
    uLongf out_len = sizeof out;
    if (compress((Bytef *)out, &out_len, (const Bytef *)"hello", 5) != Z_OK) {
        return 1;
    }
    printf("compressed %lu bytes into %lu\n", 5UL, (unsigned long)out_len);
    return 0;
}
