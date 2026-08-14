# BAD: silent truncation. A 64-bit quantity is read through a 32-bit load, dropping
# the upper 32 bits. This assembles cleanly, so the bug is invisible until a caller
# depends on the full value — %rax = low 32 bits, zero-extended (see reference rule 2).
	.text
	.globl	bad_truncate
bad_truncate:
	movl	(%rdi), %eax
	ret
