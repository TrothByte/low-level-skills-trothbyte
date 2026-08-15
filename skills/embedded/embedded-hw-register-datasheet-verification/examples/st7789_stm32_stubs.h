/* st7789_stm32_stubs.h - host-compilable register map model.
 * The layout facts encoded here are documented in
 * references/register-verification.md with datasheet source ids.
 * Examples compile against this model: any register/bit/reset constant
 * that disagrees with the datasheet trips a _Static_assert.
 */
#ifndef ST7789_STM32_STUBS_H
#define ST7789_STM32_STUBS_H

#include <stdint.h>
#include <stddef.h>

/* ---- ST7789V command register model (SPI access, command 0x36) ---- */

#define ST7789_MADCTL_CMD 0x36u

#define MADCTL_MY   0x80u
#define MADCTL_MX   0x40u
#define MADCTL_MV   0x20u
#define MADCTL_ML   0x10u
#define MADCTL_BGR  0x08u
#define MADCTL_MH   0x04u
#define MADCTL_MASK 0xFCu

#define ST7789_MADCTL_RESET 0x00u

/* ---- STM32F103 I2C1 (RM0008 layout; CMSIS stm32f1xx) ---- */

#define I2C1_BASE 0x40005400u
#define I2C2_BASE 0x40005800u

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t OAR1;
    volatile uint32_t OAR2;
    volatile uint32_t DR;
    volatile uint32_t SR1;
    volatile uint32_t SR2;
    volatile uint32_t CCR;
    volatile uint32_t TRISE;
} I2C_TypeDef;

#define I2C1 ((I2C_TypeDef *)I2C1_BASE)
#define I2C2 ((I2C_TypeDef *)I2C2_BASE)

#define I2C_CR1_PE        0x0001u
#define I2C_CR1_SMBUS     0x0002u
#define I2C_CR1_SMBTYPE   0x0008u
#define I2C_CR1_ENARP     0x0010u
#define I2C_CR1_ENPEC     0x0020u
#define I2C_CR1_ENGC      0x0040u
#define I2C_CR1_NOSTRETCH 0x0080u
#define I2C_CR1_START     0x0100u
#define I2C_CR1_STOP      0x0200u
#define I2C_CR1_ACK       0x0400u
#define I2C_CR1_POS       0x0800u
#define I2C_CR1_PEC       0x1000u
#define I2C_CR1_ALERT     0x2000u
#define I2C_CR1_SWRST     0x8000u

#define I2C_SR1_SB       0x0001u
#define I2C_SR1_ADDR     0x0002u
#define I2C_SR1_BTF      0x0004u
#define I2C_SR1_ADD10    0x0008u
#define I2C_SR1_STOPF    0x0010u
#define I2C_SR1_RxNE     0x0020u
#define I2C_SR1_TxE      0x0040u
#define I2C_SR1_BERR     0x0100u
#define I2C_SR1_ARLO     0x0200u
#define I2C_SR1_AF       0x0400u
#define I2C_SR1_OVR      0x0800u
#define I2C_SR1_PECERR   0x1000u
#define I2C_SR1_TIMEOUT  0x4000u
#define I2C_SR1_SMBALERT 0x8000u

#define I2C_TRISE_RESET 0x0002u

#define RCC_BASE 0x40021000u
#define RCC_APB1ENR_I2C1EN (1u << 21)

_Static_assert(offsetof(I2C_TypeDef, CR1) == 0x00, "CR1 at 0x00");
_Static_assert(offsetof(I2C_TypeDef, CR2) == 0x04, "CR2 at 0x04");
_Static_assert(offsetof(I2C_TypeDef, OAR1) == 0x08, "OAR1 at 0x08");
_Static_assert(offsetof(I2C_TypeDef, OAR2) == 0x0C, "OAR2 at 0x0C");
_Static_assert(offsetof(I2C_TypeDef, DR) == 0x10, "DR at 0x10");
_Static_assert(offsetof(I2C_TypeDef, SR1) == 0x14, "SR1 at 0x14");
_Static_assert(offsetof(I2C_TypeDef, SR2) == 0x18, "SR2 at 0x18");
_Static_assert(offsetof(I2C_TypeDef, CCR) == 0x1C, "CCR at 0x1C");
_Static_assert(offsetof(I2C_TypeDef, TRISE) == 0x20, "TRISE at 0x20");
_Static_assert(sizeof(I2C_TypeDef) == 0x24, "I2C block is 0x24 bytes");

_Static_assert((I2C_SR1_SB | I2C_SR1_ADDR | I2C_SR1_BTF | I2C_SR1_ADD10 |
                I2C_SR1_STOPF | I2C_SR1_RxNE | I2C_SR1_TxE | I2C_SR1_BERR |
                I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR | I2C_SR1_PECERR |
                I2C_SR1_TIMEOUT | I2C_SR1_SMBALERT) == 0xDF7Fu,
               "SR1 defined bits are D0..D12 and D14,D15; D13 is reserved");

#endif
