# BAD: operand-size mismatch. `movl` (32-bit load) with a 16-bit register does not assemble.
# `as` error: "incorrect register `%ax' used with `l' suffix".
# The size suffix must match the register width — see good/sizes.s.
	.text
	.globl	bad_sizes
bad_sizes:
	movl	(%rdi), %ax
	ret
