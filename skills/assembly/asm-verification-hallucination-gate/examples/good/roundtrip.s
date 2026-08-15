# GOOD: the byte-level round-trip. This file is the OUTPUT of the gate
# procedure: assemble -> objdump -> compare bytes. Each instruction's bytes
# are recorded in evals/README.md under "Verified facts".
	.text
	.globl	roundtrip
roundtrip:
	pushq	%rbp
	movq	%rsp, %rbp
	movl	$38, %eax		# b8 26 00 00 00
	imull	$38, %eax, %eax		# 6b c0 26
	movq	%rbp, %rsp
	popq	%rbp
	ret				# c3
