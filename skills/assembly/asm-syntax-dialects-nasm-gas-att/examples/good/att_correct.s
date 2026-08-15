# GOOD: correct AT&T syntax. Source first, destination last; '$' marks
# immediates; memory operands use parentheses. Compare with bad/att_order.s.
	.text
	.globl	f
f:
	movl	$0, -4(%rbp)	# c7 45 fc 00 00 00 00 — store 0
	movl	$5, %eax	# b8 05 00 00 00 — immediate 5, not address 5
	ret
