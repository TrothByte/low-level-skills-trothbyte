# GOOD: compare first, then branch with the condition that matches the operand
# signedness. `cmp %rsi,%rdi` computes rdi - rsi and sets flags (reference rule 9).
	.text
	.globl	good_signed_cmp
good_signed_cmp:
	cmpq	%rsi, %rdi
	jl	.is_neg		# signed less-than (SF != OF)
	xorl	%eax, %eax
	ret
.is_neg:
	movl	$1, %eax
	ret

# For unsigned operands `jb` (CF) is the correct choice.
	.text
	.globl	good_unsigned_cmp
good_unsigned_cmp:
	cmpq	%rsi, %rdi
	jb	.below		# unsigned below (CF = 1)
	xorl	%eax, %eax
	ret
.below:
	movl	$1, %eax
	ret
