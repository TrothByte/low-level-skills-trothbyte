/*
 * BAD: // intentionally incorrect — secret-dependent integer division.
 * On targets where division latency depends on the operand values (e.g. some
 * x86/ARM implementations of unsigned/signed division), the number of cycles
 * varies with the secret dividend/divisor, leaking bits through timing.
 * The ML-DSA CVE-2026-22705 class used similar UDIV/SDIV value-dependence.
 */
#include <stdint.h>

static uint32_t secret_scaled(uint32_t secret) {
    uint32_t scale = 1 + (secret & 7u); /* value-dependent divisor */
    return 1000000000u / scale;         /* timing varies with `scale` */
}

uint32_t serialize_secret(uint32_t secret) {
    return secret_scaled(secret);
}
