# intentionally incorrect
# BAD: Intel-syntax operand order in AT&T. The agent wants "store 0 to [rbp-4]"
# (Intel: mov DWORD [rbp-4], 0) but writes AT&T with dest first and no '$'.
# "movl 0x0, -0x4(%rbp)" tries to copy between TWO memory operands — invalid.
	.text
	.globl	f
f:
	pushq	%rbp
	movq	%rsp, %rbp
	movl	0x0, -0x4(%rbp)
	popq	%rbp
	ret
