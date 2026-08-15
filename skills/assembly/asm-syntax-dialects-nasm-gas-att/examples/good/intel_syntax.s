# GOOD: GAS Intel-syntax mode (.intel_syntax noprefix). Destination comes
# first, size is spelled PTR, immediates need no marker. The same instruction
# set as AT&T — only the textual dialect changes.
	.intel_syntax noprefix
	.text
	.globl	f
f:
	mov	eax, 5		# b8 05 00 00 00
	mov	DWORD PTR [rbp-4], 0	# c7 45 fc 00 00 00 00
	ret
