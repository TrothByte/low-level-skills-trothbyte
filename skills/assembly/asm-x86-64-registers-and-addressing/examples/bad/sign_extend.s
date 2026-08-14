# BAD: `movl` (32-bit) zero-extends to 64 bits. A negative int32 in memory
# (e.g. 0xFFFFFFFF) becomes 0x00000000FFFFFFFF in %rax, NOT sign-extended -1.
# This assembles cleanly; the bug appears when the caller treats %rax as signed
# 64-bit (reference rules 2 and 8).
	.text
	.globl	bad_sign_extend
bad_sign_extend:
	movl	(%rdi), %eax
	ret
