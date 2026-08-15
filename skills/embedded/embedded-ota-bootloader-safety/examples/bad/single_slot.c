/*
 * BAD: // intentionally incorrect — single-slot OTA with no rollback.
 * The new image overwrites the only slot; a failed/corrupt write leaves no
 * fallback and the bootloader has nothing to roll back to. MCUboot/ESP-IDF
 * require an A/B layout. Host-runnable model of the decision logic.
 */
#include <stdio.h>

static int active_slot_valid;
static int new_image_verified;

static void flash_update_single_slot(void) {
    /* BUG: erasing and writing the ONLY slot; power cut mid-write =
       no valid image remains */
    active_slot_valid = 0;
    active_slot_valid = new_image_verified;   /* hope it lands whole */
}

int main(void) {
    new_image_verified = 1;   /* checksum ok */
    flash_update_single_slot();
    printf("single-slot update: active_slot_valid=%d (no rollback path)\n",
           active_slot_valid);
    return 0;
}
