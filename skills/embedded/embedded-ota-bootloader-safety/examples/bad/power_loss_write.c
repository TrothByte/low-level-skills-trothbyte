/*
 * BAD: // intentionally incorrect — power-loss-unsafe single-shot slot write.
 * erase + write of the active slot in one step: a power cut between the two
 * leaves a corrupt active image and the device cannot boot. The safe pattern
 * writes to a standby slot with status flags.
 */
#include <stdio.h>
#include <string.h>

static unsigned char slot[64];

static void write_image(unsigned char img[64]) {
    memset(slot, 0xFF, sizeof(slot));      /* erase active slot */
    memcpy(slot, img, sizeof(slot));       /* write image */
    /* power cut here -> partially written active slot, no recovery */
}

int main(void) {
    unsigned char img[64];
    memset(img, 0x5A, sizeof(img));
    write_image(img);
    printf("write reported complete (but power-cut leaves brick)\n");
    return 0;
}
