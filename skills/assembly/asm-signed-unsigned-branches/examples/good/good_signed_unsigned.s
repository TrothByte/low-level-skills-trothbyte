# GOOD examples: correct signed/unsigned x86-64 AT&T assembly.
# Mirrors the exact sequences gcc 16.1 emits for the corresponding C.
# Assemble with `gcc -c`.

.text

# GOOD 1: unsigned comparison with the unsigned mnemonic jae.
# Equivalent C: unsigned a, b; a >= b.
# Uses the carry flag: jae = CF==0 (no borrow), correct for values >= 2^31.
.globl good_unsigned_jae
good_unsigned_jae:
    movl    %edi, %eax
    cmpl    %esi, %eax
    jae     .Ltrue_1        # unsigned above-or-equal
    xorl    %eax, %eax
    ret
.Ltrue_1:
    movl    $1, %eax
    ret

# GOOD 2: unsigned strict-less-than with jb.
# Equivalent C: unsigned a, b; a < b.
.globl good_unsigned_jb
good_unsigned_jb:
    movl    %edi, %eax
    cmpl    %esi, %eax
    jb      .Ltrue_2        # unsigned below
    xorl    %eax, %eax
    ret
.Ltrue_2:
    movl    $1, %eax
    ret

# GOOD 3: signed arithmetic right shift with sar.
# Equivalent C: int a; a >> 4 (arithmetic, preserves the sign bit).
# gcc emits `sarl $4, %eax`; -16 >> 4 == -1.
.globl good_signed_sar
good_signed_sar:
    movl    %edi, %eax
    sarl    $4, %eax        # arithmetic shift, sign-filled
    ret

# GOOD 4: signed division with cdq then idiv.
# Equivalent C: int a, b; a / b. gcc emits `cltd` (cdq) + `idivl`.
.globl good_idiv_cdq
good_idiv_cdq:
    movl    %edi, %eax
    movl    %esi, %ecx
    cltd                    # cdq: sign-extend eax into edx
    idivl   %ecx            # edx:eax / ecx, signed
    ret

# GOOD 5: unsigned division with zeroed high half then div.
# Equivalent C: unsigned a, b; a / b. gcc emits `xorl %edx, %edx` + `divl`.
.globl good_div_zeroed
good_div_zeroed:
    movl    %edi, %eax
    movl    %esi, %ecx
    xorl    %edx, %edx      # high half = 0 (unsigned)
    divl    %ecx            # edx:eax / ecx, unsigned
    ret

# GOOD 6: signed widening load with movsx.
# Equivalent C: int e(short s) { return s; } -> movswl %cx, %eax.
.globl good_movsx_signed
good_movsx_signed:
    movswl  %cx, %eax       # sign-extend short to int
    ret

# GOOD 7: unsigned widening load with movzx.
# Equivalent C: int eu(unsigned short s) { return (int)s; } -> movzwl.
.globl good_movzx_unsigned
good_movzx_unsigned:
    movzwl  %cx, %eax       # zero-extend unsigned short to int
    ret
