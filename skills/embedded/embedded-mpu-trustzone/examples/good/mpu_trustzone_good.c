/*
 * GOOD: ARMv8-M (Cortex-M33) MPU + TrustZone setup, CMSIS-style.
 *
 * Compiles and runs on the host: the MPU/SAU register blocks are modeled as
 * plain structs and the asserts validate the configuration a target would
 * accept. On silicon the blocks live in the PPB at 0xE000ED90 (MPU) and
 * 0xE000EDD0 (SAU, secure alias).
 *
 * Target build (requires an ARM compiler):
 *   arm-none-eabi-gcc -mcpu=cortex-m33 -mthumb -mcmse -Wall -Wextra -Werror -O2 -c
 *
 * The host gcc in this repository is x86-only, so the ARM-only parts
 * (cmse_nonsecure_entry / cmse_nonsecure_call) are guarded by the CMSE
 * feature macro and compile as plain C here.
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
#define SAU_RBAR_BADDR_Pos      5U
#define SAU_RLAR_LADDR_Pos      5U
#define SAU_RLAR_NSC_Pos        1U
#define SAU_RLAR_ENABLE_Pos     0U

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

/* ---- MAIR attribute encodings (mirror CMSIS-5 mpu_armv8.h) ---- */
#define TZ_ATTR_DEVICE_NGNRNE 0x00U  /* Device, non-gathering/non-reordering */
#define TZ_ATTR_NORMAL_WB_WRA 0xFFU  /* Normal, write-back, read+write alloc */

/* ---- example memory map (must match the linker scripts) ---- */
#define TZ_ADDR_FLASH        0x00000000U  /* Secure flash: bootloader + services */
#define TZ_SIZE_FLASH        0x00100000U
#define TZ_ADDR_NSC          0x00100000U  /* NSC veneers (secure flash, marked NSC) */
#define TZ_SIZE_NSC          0x00000800U
#define TZ_ADDR_FLASH_NS     0x00140000U  /* Non-secure application flash */
#define TZ_SIZE_FLASH_NS     0x00080000U
#define TZ_ADDR_SRAM_SEC     0x20000000U  /* Secure SRAM (stacks, secure context) */
#define TZ_SIZE_SRAM_SEC     0x00020000U
#define TZ_ADDR_SRAM_NS      0x20040000U  /* Non-secure SRAM */
#define TZ_SIZE_SRAM_NS      0x00020000U
#define TZ_ADDR_UART         0x40004000U  /* Device MMIO */
#define TZ_SIZE_UART         0x00000200U

static int is_power_of_two(uint32_t n)
{
    return (n != 0U) && ((n & (n - 1U)) == 0U);
}

/* ARMv8-M rule: region size must be a power of two (>= 32 B, the granularity
 * of the BASE/LIMIT fields) and the base must be aligned to the size. */
static int region_ok(uint32_t base, uint32_t size)
{
    return is_power_of_two(size) && (size >= 32U) && ((base & (size - 1U)) == 0U);
}

/* Program one ARMv8-M MPU region: RBAR/RLAR + MAIR attribute index.
 * limit = base + size - 1; RLAR stores limit>>5 (low bits ones-extended). */
static void mpu_set_region(uint32_t rnr, uint32_t base, uint32_t size,
                           uint32_t attr_idx, uint32_t ap, uint32_t xn)
{
    uint32_t limit;
    assert(region_ok(base, size));
    limit = base + size - 1U;
    MPU.RNR = rnr;
    MPU_RBAR_ACTIVE = (base & ~0x1FU) | (ap << MPU_RBAR_AP_Pos) | (xn << MPU_RBAR_XN_Pos);
    MPU_RLAR_ACTIVE = ((limit >> MPU_RLAR_LIMIT_Pos) << MPU_RLAR_LIMIT_Pos)
                    | (attr_idx << MPU_RLAR_AttrIndx_Pos)
                    | MPU_RLAR_EN;
}

/* AP encoding for the ARMv8-M RBAR: bit1 = read-only, bit0 = unprivileged
 * allowed. TZ_AP_FULL = RW any privilege; TZ_AP_PRIV = RW privileged only. */
#define TZ_AP_FULL 0x01U
#define TZ_AP_PRIV 0x00U

static void mpu_setup(void)
{
    /* MAIR slots: index 0 = normal write-back, index 1 = Device. */
    MPU.MAIR0 = (TZ_ATTR_NORMAL_WB_WRA << MPU_MAIR0_Attr0_Pos)
              | (TZ_ATTR_DEVICE_NGNRNE << MPU_MAIR0_Attr1_Pos);

    mpu_set_region(0U, TZ_ADDR_FLASH,    TZ_SIZE_FLASH,    0U, TZ_AP_FULL, 0U);
    mpu_set_region(1U, TZ_ADDR_NSC,      TZ_SIZE_NSC,      0U, TZ_AP_FULL, 0U);
    mpu_set_region(2U, TZ_ADDR_SRAM_SEC, TZ_SIZE_SRAM_SEC, 0U, TZ_AP_FULL, 1U);
    mpu_set_region(3U, TZ_ADDR_UART,     TZ_SIZE_UART,     1U, TZ_AP_PRIV, 1U);

    /* PRIVDEFENA: privileged accesses to unmapped addresses use the default
     * system map; unprivileged accesses to unmapped addresses still fault. */
    MPU.CTRL = MPU_CTRL_PRIVDEFENA | MPU_CTRL_ENABLE;
}

static void sau_setup(void)
{
    /* The SAU is a Secure-only peripheral. Program it in Secure state before
     * the first transition to Non-secure, with the SAU disabled meanwhile. */
    SAU.CTRL = 0U;

    /* Region 0: NSC veneers. Non-secure code branches here to call secure
     * functions; each entry must begin with SG (generated by the toolchain). */
    SAU.RNR = 0U;
    SAU_RBAR_ACTIVE = TZ_ADDR_NSC;
    SAU_RLAR_ACTIVE = ((TZ_ADDR_NSC + TZ_SIZE_NSC - 1U) >> SAU_RLAR_LADDR_Pos)
                      << SAU_RLAR_LADDR_Pos;
    SAU_RLAR_ACTIVE |= SAU_RLAR_NSC | SAU_RLAR_ENABLE;

    /* Region 1: Non-secure flash (application image). */
    SAU.RNR = 1U;
    SAU_RBAR_ACTIVE = TZ_ADDR_FLASH_NS;
    SAU_RLAR_ACTIVE = ((TZ_ADDR_FLASH_NS + TZ_SIZE_FLASH_NS - 1U) >> SAU_RLAR_LADDR_Pos)
                      << SAU_RLAR_LADDR_Pos;
    SAU_RLAR_ACTIVE |= SAU_RLAR_ENABLE;

    /* Region 2: Non-secure SRAM (application data). */
    SAU.RNR = 2U;
    SAU_RBAR_ACTIVE = TZ_ADDR_SRAM_NS;
    SAU_RLAR_ACTIVE = ((TZ_ADDR_SRAM_NS + TZ_SIZE_SRAM_NS - 1U) >> SAU_RLAR_LADDR_Pos)
                      << SAU_RLAR_LADDR_Pos;
    SAU_RLAR_ACTIVE |= SAU_RLAR_ENABLE;

    SAU.CTRL = SAU_CTRL_ENABLE;
}

/* Secure entry point callable from Non-secure. With CMSE the compiler emits
 * the SG veneer and the entry checks; on the host it is a plain function. */
#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
__attribute__((cmse_nonsecure_entry))
#endif
uint32_t secure_get_secret(void)
{
    return 0x5EC0FFEEU;
}

/* Function-pointer attribute for secure -> non-secure calls. With -mcmse the
 * compiler emits BLXNS plus the secure return sequence instead of a plain
 * BLX (which would run the callee in Secure state). */
#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
#define TZ_NS_CALL_ATTR __attribute__((cmse_nonsecure_call))
#else
#define TZ_NS_CALL_ATTR
#endif

typedef void (*ns_uart_send_fn_t)(uint32_t data) TZ_NS_CALL_ATTR;

static void call_nonsecure(ns_uart_send_fn_t fn, uint32_t data)
{
    fn(data);
}

static void host_ns_cb(uint32_t data)
{
    (void)data;
}

/* MMIO: volatile-qualified pointers, and the covering region is Device. */
typedef struct {
    volatile uint32_t *sr;
    volatile uint32_t *dr;
} uart_t;

static uint32_t uart_read_status(const uart_t *u) { return *u->sr; }

static void uart_write(uart_t *u, uint32_t data) { *u->dr = data; }

static void uart_send_blocking(uart_t *u, uint32_t data)
{
    while ((uart_read_status(u) & 0x40U) == 0U) { }   /* poll TX-ready */
    uart_write(u, data);
}

static void check_config(void)
{
    MPU.RNR = 0U;
    assert((MPU_RLAR_ACTIVE & MPU_RLAR_EN) != 0U);                /* region enabled */
    assert(region_ok(TZ_ADDR_FLASH, TZ_SIZE_FLASH));
    assert(region_ok(TZ_ADDR_NSC, TZ_SIZE_NSC));
    assert(region_ok(TZ_ADDR_SRAM_SEC, TZ_SIZE_SRAM_SEC));
    assert(region_ok(TZ_ADDR_UART, TZ_SIZE_UART));
    assert((MPU.CTRL & MPU_CTRL_ENABLE) != 0U);
    assert((MPU.CTRL & MPU_CTRL_PRIVDEFENA) != 0U);               /* background */
    assert(((MPU.MAIR0 >> MPU_MAIR0_Attr1_Pos) & 0xFFU) == TZ_ATTR_DEVICE_NGNRNE);
    SAU.RNR = 0U;
    assert((SAU.CTRL & SAU_CTRL_ENABLE) != 0U);                   /* SAU on */
    assert((SAU_RLAR_ACTIVE & SAU_RLAR_NSC) != 0U);               /* NSC present */
}

static void host_verify_mmio(void)
{
    volatile uint32_t sr_word = 0x40U;   /* TX-ready set */
    volatile uint32_t dr_word = 0U;
    uart_t u;
    u.sr = &sr_word;
    u.dr = &dr_word;
    uart_send_blocking(&u, 0x41U);
    assert(dr_word == 0x41U);
}

int main(void)
{
    mpu_setup();
    sau_setup();
    check_config();
    host_verify_mmio();
    call_nonsecure(host_ns_cb, 0x55U);
    assert(secure_get_secret() == 0x5EC0FFEEU);
    return 0;
}
