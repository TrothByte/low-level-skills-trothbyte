// GOOD: inline asm with operands whose width matches the intended extension.
// `movslq` sign-extends int32 -> int64 into the full 64-bit output register.
long long load_signed(const long long *p)
{
    long long r;
    asm("movslq (%1), %0" : "=r"(r) : "r"(p));
    return r;
}

// GOOD: a deliberate 32-bit load; `%k0` selects the 32-bit register name and the
// write zero-extends to the full 64-bit output (reference rule 2).
unsigned long long load_unsigned(const unsigned long long *p)
{
    unsigned long long r;
    asm("movl (%1), %k0" : "=r"(r) : "r"(p));
    return r;
}
