# GOOD: GAS mnemonics are case-insensitive — MOVL and movl both assemble to
# the same encoding. (Contrast: NASM mnemonics are case-insensitive too, but
# NASM LABELS are case-sensitive: Loop != loop != LOOP.) This file is a
# false-positive fixture: do NOT flag uppercase mnemonics in GAS.
	.text
	.globl	f
f:
	MOVL	%eax, %ebx	# 89 c3
	ret
