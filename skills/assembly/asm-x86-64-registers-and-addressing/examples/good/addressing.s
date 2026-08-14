# GOOD: valid addressing modes — base+index*scale, base+disp32, RIP-relative.
	.text
	.globl	good_addressing
good_addressing:
	movq	(%rdi, %rsi, 8), %rax	# base + index*8, no displacement
	movq	-8(%rdi), %rbx		# base + disp32 (negative, sign-extended)
	movq	my_data(%rip), %rcx	# RIP-relative load
	leaq	my_data(%rip), %rdx	# RIP-relative address
	ret

	.section .data
my_data:
	.quad	42
