# intentionally incorrect
# BAD: "AX is 8-bit" — AX is 16 bits, AL is 8. The agent wants to zero a 16-bit
# counter (%ax) but writes %al, leaving the high byte of %ax untouched.
	.text
	.globl	f
f:
	movl	$0x1234, %eax
	movb	$0, %al
	ret
