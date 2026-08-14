# GOOD: choose the extension width to match the destination.
	.text
	.globl	good_sign_extend32
good_sign_extend32:
	movslq	(%rdi), %rax		# int32 -> int64, sign-extended
	ret

	.globl	good_sign_extend8
good_sign_extend8:
	movsbq	(%rdi), %rax		# int8 -> int64, sign-extended
	ret

	.globl	good_zero_extend8
good_zero_extend8:
	movzbl	(%rdi), %eax		# uint8 -> uint64 (zero-extended to 64 by rule 2)
	ret
