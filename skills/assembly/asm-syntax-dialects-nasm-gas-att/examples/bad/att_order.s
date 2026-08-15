# intentionally incorrect
# BAD: Intel-syntax operand order + missing '$' in GAS AT&T. The agent wants
# "store 0 to [rbp-4]" (Intel: mov DWORD PTR [rbp-4], 0) but writes it with
# destination first and no immediate marker. GAS rejects it: two memory operands.
	.text
	.globl	f
f:
	pushq	%rbp
	movq	%rsp, %rbp
	movl	0x0, -0x4(%rbp)
	popq	%rbp
	ret
