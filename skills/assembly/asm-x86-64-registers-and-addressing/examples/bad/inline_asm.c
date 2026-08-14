// BAD: 32-bit load into a 64-bit output operand. `movl` zero-extends, so a
// negative int32 becomes a huge positive value (0xFFFFFFFF -> 0x00000000FFFFFFFF).
// Compiles cleanly with -Werror; the bug is semantic (reference rules 2 and 8).
// Note: `long long` (not `long`) is used because Windows is LLP64 — `long` is 32-bit.
// `%k0` prints the 32-bit name of the output register; a bare `%0` would be `%rax`.
long long load_low(const long long *p)
{
    long long r;
    asm("movl (%1), %k0" : "=r"(r) : "r"(p));
    return r;
}

// BAD: `movsbl` sign-extends only to 32 bits; byte 0xFF becomes 0x00000000FFFFFFFF.
// Use `movsbq` when the destination is 64-bit (reference rule 8).
long long load_byte(const unsigned char *p)
{
    long long r;
    asm("movsbl (%1), %k0" : "=r"(r) : "r"(p));
    return r;
}
