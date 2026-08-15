# intentionally incorrect
# BAD: `$` missing on an immediate. `movl 5, %eax` treats 5 as an absolute
# memory address (5), not the constant 5. GAS may accept it as a displacement;
# at runtime it loads from address 0x5, not value 5.
	.text
	.globl	f
f:
	movl	5, %eax
	ret
