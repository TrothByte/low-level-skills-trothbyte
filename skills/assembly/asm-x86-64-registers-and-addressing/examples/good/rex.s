# GOOD: the assembler emits the required REX prefixes; verify them with objdump.
	.text
	.globl	good_rex
good_rex:
	movq	%r9, %r8		# 4d 89 c8 (REX.W + REX.R + REX.B)
	movl	$1, %r8d		# 41 b8 01 00 00 00 (REX.B)
	movzbl	(%rdi), %r8d		# 44 0f b6 07 (REX.R)
	ret
