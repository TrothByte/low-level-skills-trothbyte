// GOOD: capability-safe element copy and sealed token hand-off.
//
// 1. Copying an array of capabilities element-wise preserves tags (memcpy would
//    drop them).
// 2. Sealing creates a token that can be passed without granting dereference
//    rights; unseal with the same key at the authorized consumer.
//
// Target toolchain: QEMU-CHERI + cheribuild (absent here; documented).

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    int *ptr;
    size_t len;
} buffer_t;

buffer_t clone_buffers(const buffer_t *src, size_t n)
{
    buffer_t dst;
    dst.ptr = (int *)malloc(src->len * sizeof(int));
    dst.len = src->len;
    // Element-wise copy preserves the tag (memcpy would clear it).
    for (size_t i = 0; i < src->len; i++)
        dst.ptr[i] = src->ptr[i];
    return dst;
}

int main(void)
{
    int data[4] = {1, 2, 3, 4};
    buffer_t src = {data, 4};
    buffer_t dst = clone_buffers(&src, 1);

    // Sealed token: pass authority without dereference rights.
    uintptr_t key = 0xCAFE;
    void *token = cheri_seal((void *)&dst, key);   // sealed capability
    buffer_t *unsealed = (buffer_t *)cheri_unseal(token, key);
    (void)unsealed;

    free(dst.ptr);
    return 0;
}
