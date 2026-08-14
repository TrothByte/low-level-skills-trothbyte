# BAD 1: `lea` does NOT set flags. The `jz` below tests flags left by whatever ran
# before this function — the branch target depends on stale, unrelated state
# (reference rule 9).
	.text
	.globl	bad_lea_flags
bad_lea_flags:
	leaq	(%rdi, %rsi), %rax
	jz	.zero_sum
	movq	$1, %rax
	ret
.zero_sum:
	xorq	%rax, %rax
	ret

# BAD 2: `jb` is the UNSIGNED below-branch (tests CF). On signed operands the result
# is wrong: -1 vs 1 is "less" signed, but unsigned 0xFF..F > 1 sets CF=0, so `jb`
# does not branch where `jl` should.
	.text
	.globl	bad_signed_cmp
bad_signed_cmp:
	cmpq	%rsi, %rdi
	jb	.is_neg		# unsigned branch on signed values
	xorl	%eax, %eax
	ret
.is_neg:
	movl	$1, %eax
	ret
