/*
 * BAD: // intentionally incorrect — invented register bit (not in datasheet).
 * RCC_CFGR's USBPRE bit does not exist on many STM32 parts; the code compiles
 * because the CMSIS header for another part defines it. On the real part the
 * write is silently ignored. The correct action is to verify the bit in the
 * reference manual for the exact part, or use the vendor HAL.
 */
#include <stdint.h>

#define RCC_CFGR (*(volatile uint32_t *)0x40023808UL)
#define RCC_CFGR_USBPRE (1u << 22)   /* not present on the target part */

static void init_usb_div_wrong(void) {
    RCC_CFGR |= RCC_CFGR_USBPRE;    /* compiles, ignored on the target */
}
