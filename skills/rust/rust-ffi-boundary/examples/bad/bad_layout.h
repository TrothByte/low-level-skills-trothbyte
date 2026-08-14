#ifndef BAD_LAYOUT_H
#define BAD_LAYOUT_H

#include <stdint.h>

/* C side is packed: no padding anywhere. sizeof(struct msg) == 9. */
#pragma pack(push, 1)
struct msg {
    uint32_t magic;
    uint8_t ver;
    uint32_t len;
};
#pragma pack(pop)

#endif
