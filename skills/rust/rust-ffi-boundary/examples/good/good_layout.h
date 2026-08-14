#ifndef GOOD_LAYOUT_H
#define GOOD_LAYOUT_H

#include <stdint.h>

struct header {
    uint32_t magic;
    uint8_t ver;
    uint8_t reserved[2];
    uint32_t len;
};

#endif
