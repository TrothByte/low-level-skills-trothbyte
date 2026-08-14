/*
 * BAD: the same ARMv8-M (Cortex-M33) MPU + TrustZone setup, with the classic
 * mistakes (B1..B7). It compiles cleanly — that is the trap — but fails the
 * host invariant checks below and would fault on target.
 *
 * B1  misconfigured region base: 0x20002000 is not aligned to the 0x20000 size
 * B2  RLAR enable bit never set (region is inert even though "configured")
 * B3  MPU_CTRL.PRIVDEFENA missing: privileged unmapped accesses will fault
 * B4  SAU never configured: all memory stays Secure, the NS image cannot run
 * B5  secure -> non-secure call without cmse_nonsecure_call (plain BLX)
 * B6  UART region uses a cacheable Normal attribute (attr slot 1 set to WB)
 * B7  MMIO pointers are not volatile
 *
 * Target build (requires an ARM compiler):
 *   arm-none-eabi-gcc -mcpu=cortex-m33 -mthumb -mcmse -Wall -Wextra -Werror -O2 -c
 */
#include <stdint.h>
#include <assert.h>

/* ---- CMSIS-style register blocks (mirror CMSIS-5 core_cm33.h) ----
 * On the target, RBAR/RLAR are single registers whose access is redirected
 * to the region selected by RNR; the arrays below are the faithful host
 * model of that redirection, so RNR read-back behaves like hardware. */
typedef struct {
    volatile uint32_t TYPE;    /* 0x000 */
    volatile uint32_t CTRL;    /* 0x004 */
    volatile uint32_t RNR;     /* 0x008 */
    volatile uint32_t RBAR[8]; /* 0x00C, RNR-redirected */
    volatile uint32_t RLAR[8]; /* 0x010, RNR-redirected */
    volatile uint32_t MAIR0;   /* 0x030 */
    volatile uint32_t MAIR1;   /* 0x034 */
} MPU_Type;

typedef struct {
    volatile uint32_t CTRL;    /* 0x000 */
    volatile uint32_t TYPE;    /* 0x004 */
    volatile uint32_t RNR;     /* 0x008 */
    volatile uint32_t RBAR[8]; /* 0x00C, RNR-redirected */
    volatile uint32_t RLAR[8]; /* 0x010, RNR-redirected */
} SAU_Type;

static MPU_Type MPU;
static SAU_Type SAU;

/* ---- bit positions (mirror CMSIS-5 core_cm33.h) ---- */
#define MPU_CTRL_PRIVDEFENA_Pos 2U
#define MPU_CTRL_ENABLE_Pos     0U
#define MPU_RBAR_AP_Pos         1U
#define MPU_RBAR_XN_Pos         0U
#define MPU_RLAR_LIMIT_Pos      5U
#define MPU_RLAR_AttrIndx_Pos   1U
#define MPU_RLAR_EN_Pos         0U
#define MPU_MAIR0_Attr0_Pos     0U
#define MPU_MAIR0_Attr1_Pos     8U
#define SAU_CTRL_ENABLE_Pos     0U
#define SAU_RLAR_NSC_Pos        1U

#define MPU_CTRL_PRIVDEFENA (1U << MPU_CTRL_PRIVDEFENA_Pos)
#define MPU_CTRL_ENABLE     (1U << MPU_CTRL_ENABLE_Pos)
#define MPU_RLAR_EN         (1U << MPU_RLAR_EN_Pos)
#define SAU_CTRL_ENABLE     (1U << SAU_CTRL_ENABLE_Pos)
#define SAU_RLAR_NSC        (1U << SAU_RLAR_NSC_Pos)
#define SAU_RLAR_ENABLE     (1U << SAU_RLAR_ENABLE_Pos)

/* RNR-redirected access: the region the RBAR/RLAR registers currently see. */
#define MPU_RBAR_ACTIVE (MPU.RBAR[MPU.RNR & 7U])
#define MPU_RLAR_ACTIVE (MPU.RLAR[MPU.RNR & 7U])
#define SAU_RBAR_ACTIVE (SAU.RBAR[SAU.RNR & 7U])
#define SAU_RLAR_ACTIVE (SAU.RLAR[SAU.RNR & 7U])

static int is_power_of_two(uint32_t n)
{
    return (n != 0U) && ((n & (n - 1U)) == 0U);
}

static int region_ok(uint32_t base, uint32_t size)
{
    return is_power_of_two(size) && (size >= 32U) && ((base & (size - 1U)) == 0U);
}

/* Writes RBAR/RLAR without validation and without the EN bit (B2): the
 * registers are written, but the region never becomes active. */
static void mpu_set_region(uint32_t rnr, uint32_t base, uint32_t size,
                           uint32_t attr_idx, uint32_t ap, uint32_t xn)
{
    uint32_t limit = base + size - 1U;
    MPU.RNR = rnr;
    MPU_RBAR_ACTIVE = (base & ~0x1FU) | (ap << MPU_RBAR_AP_Pos) | (xn << MPU_RBAR_XN_Pos);
    MPU_RLAR_ACTIVE = ((limit >> MPU_RLAR_LIMIT_Pos) << MPU_RLAR_LIMIT_Pos)
                    | (attr_idx << MPU_RLAR_AttrIndx_Pos);   /* B2: no MPU_RLAR_EN */
}

#define TZ_AP_FULL 0x01U
#define TZ_AP_PRIV 0x00U

static void mpu_setup(void)
{
    /* B6: both MAIR slots are Normal write-back; the UART region below will
     * use slot 1, making a Device peripheral look cacheable. */
    MPU.MAIR0 = (0xFFU << MPU_MAIR0_Attr0_Pos) | (0xFFU << MPU_MAIR0_Attr1_Pos);

    mpu_set_region(0U, 0x00000000U, 0x00100000U, 0U, TZ_AP_FULL, 0U);
    mpu_set_region(1U, 0x00100000U, 0x00000800U, 0U, TZ_AP_FULL, 0U);
    mpu_set_region(2U, 0x20002000U, 0x00020000U, 0U, TZ_AP_FULL, 1U); /* B1 */
    mpu_set_region(3U, 0x40004000U, 0x00000200U, 1U, TZ_AP_PRIV, 1U); /* B6 */

    MPU.CTRL = MPU_CTRL_ENABLE;   /* B3: PRIVDEFENA never set */
}

/* B5: no cmse_nonsecure_call on the function-pointer type, so the secure
 * image calls the non-secure function with a plain BLX and it runs in the
 * Secure state. */
typedef void (*ns_uart_send_fn_t)(uint32_t data);

static void call_nonsecure(ns_uart_send_fn_t fn, uint32_t data)
{
    fn(data);
}

static void host_ns_cb(uint32_t data)
{
    (void)data;
}

/* B7: register pointers are not volatile-qualified. */
typedef struct {
    uint32_t *sr;
    uint32_t *dr;
} uart_t;

static uint32_t uart_read_status(const uart_t *u) { return *u->sr; }

static void uart_write(uart_t *u, uint32_t data) { *u->dr = data; }

static void uart_send_blocking(uart_t *u, uint32_t data)
{
    while ((uart_read_status(u) & 0x40U) == 0U) { }
    uart_write(u, data);
}

/* The checks an agent should run before trusting the setup. Each failing
 * assert maps to a bug above; the run aborts at the first one. */
static void check_config(void)
{
    assert(region_ok(0x20002000U, 0x00020000U));                   /* B1 */
    MPU.RNR = 0U;
    assert((MPU_RLAR_ACTIVE & MPU_RLAR_EN) != 0U);                 /* B2 */
    assert((MPU.CTRL & MPU_CTRL_PRIVDEFENA) != 0U);                /* B3 */
    assert((SAU.CTRL & SAU_CTRL_ENABLE) != 0U);                    /* B4 */
    SAU.RNR = 0U;
    assert((SAU_RLAR_ACTIVE & SAU_RLAR_NSC) != 0U);                /* B4 */
    assert(((MPU.MAIR0 >> MPU_MAIR0_Attr1_Pos) & 0xFFU) == 0x00U); /* B6 */
}

int main(void)
{
    uint32_t sr_word = 0x40U;
    uint32_t dr_word = 0U;
    uart_t u;
    mpu_setup();
    u.sr = &sr_word;
    u.dr = &dr_word;
    uart_send_blocking(&u, 0x41U);
    call_nonsecure(host_ns_cb, 0x55U);
    check_config();
    return 0;
}
