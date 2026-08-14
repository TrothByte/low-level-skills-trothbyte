# GOOD: use the encoding that matches the intended 64-bit value.
	.text
	.globl	good_imm_minus1
good_imm_minus1:
	movq	$-1, %rax		# 48 c7 c0 ff ff ff ff — compact, correct for -1
	ret

	.globl	good_imm_u32
good_imm_u32:
	movl	$0xFFFFFFFF, %eax	# b8 ff ff ff ff — zero-extends to 0x00000000FFFFFFFF
	ret

	.globl	good_imm_u64
good_imm_u64:
	movabsq	$0xFFFFFFFF, %rax	# 48 b8 ff ff ff ff 00 00 00 00 — exact 64-bit constant
	ret
