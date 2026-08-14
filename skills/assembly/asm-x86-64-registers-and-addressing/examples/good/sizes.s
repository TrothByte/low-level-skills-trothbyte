# GOOD: operand-size suffix matches the register width; full 64-bit loads keep all bits.
	.text
	.globl	good_load64
good_load64:
	movq	(%rdi), %rax
	ret

# Deliberate 32-bit load: the write to %eax zero-extends to %rax (reference rule 2).
	.globl	good_load32
good_load32:
	movl	(%rdi), %eax
	ret
