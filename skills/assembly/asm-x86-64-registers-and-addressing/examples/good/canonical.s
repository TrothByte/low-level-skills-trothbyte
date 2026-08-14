# GOOD: addresses stay in the canonical user range (bit 47 = 0, upper bits zero).
	.text
	.globl	good_canonical
good_canonical:
	movq	(%rdi), %rax		# caller-supplied pointer — canonical by contract
	ret

	.globl	good_canonical_lea
good_canonical_lea:
	leaq	my_data(%rip), %rax	# assembler resolves a canonical address
	ret

	.section .data
my_data:
	.quad	42
