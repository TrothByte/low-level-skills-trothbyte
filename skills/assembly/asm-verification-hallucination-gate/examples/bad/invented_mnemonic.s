# intentionally incorrect
# BAD: "movqad" is an invented mnemonic — it does not exist in the Intel SDM.
# The assembler rejects it, which is exactly what a hallucination gate needs
# to surface. Never ship assembly whose mnemonic you cannot cite from the SDM.
	.text
	.globl	f
f:
	movqad	%rax, %rbx
	ret
