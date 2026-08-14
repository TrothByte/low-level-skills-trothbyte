# BAD examples: signed/unsigned mistakes in x86-64 AT&T assembly.
# These compile but produce wrong results for specific inputs.
# Teaching comments explain each error; assemble-only with `gcc -c`.

.text

# BAD 1: unsigned comparison using the SIGNED mnemonic jge.
# Intent: return 1 if (unsigned)a >= (unsigned)b.
# Problem: jge decodes SF==OF. Unsigned semantics need the carry flag (jae).
# For a = -1 (0xffffffff), b = 2: signed view says a < b, unsigned view says
# a > b. jge takes the wrong path for values above INT_MAX.
.globl bad_unsigned_jge
bad_unsigned_jge:
    movl    %edi, %eax
    cmpl    %esi, %eax
    jge     .Ltrue_1        # WRONG: signed greater-or-equal
    xorl    %eax, %eax
    ret
.Ltrue_1:
    movl    $1, %eax
    ret

# BAD 2: logical right shift (shr) on a signed value.
# Intent: signed arithmetic shift, a >> 4 (rounds toward negative infinity).
# Problem: shrl fills the high bits with zeros. For a = -16 (0xfffffff0) the
# correct result is -1 (0xffffffff); shrl gives 0x0fffffff (268435455).
.globl bad_signed_shr
bad_signed_shr:
    movl    %edi, %eax
    shrl    $4, %eax        # WRONG: logical shift, sign lost
    ret

# BAD 3: signed idiv without cdq.
# Intent: signed quotient a / b.
# Problem: idiv divides EDX:EAX by the divisor. EDX is never set, so it holds
# whatever the caller left (caller-saved). If EDX != 0 the quotient is huge and
# wrong, or the CPU raises #DE. With EDX == 0 it appears to work by luck.
.globl bad_idiv_no_cdq
bad_idiv_no_cdq:
    movl    %edi, %eax
    movl    %esi, %ecx
    idivl   %ecx            # WRONG: EDX not sign-extended
    ret

# BAD 4: unsigned div with a stale high half.
# Intent: unsigned quotient a / b.
# Problem: div also reads EDX:EAX; EDX may be nonzero garbage from a previous
# operation, so the quotient is wrong for large dividends.
.globl bad_div_stale_edx
bad_div_stale_edx:
    movl    %edi, %eax
    movl    %esi, %ecx
    divl    %ecx            # WRONG: EDX not zeroed
    ret

# BAD 5: zero-extending load of a signed value.
# Intent: widen (short)s to int, preserving the sign.
# Problem: movzwl zero-extends; (short)-1 (0xffff) becomes 65535, not -1.
.globl bad_movzx_signed
bad_movzx_signed:
    movzwl  %cx, %eax       # WRONG: sign lost
    ret
