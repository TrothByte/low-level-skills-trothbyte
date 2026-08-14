# BAD: forming a non-canonical address. Only bits 47:0 matter for canonicality —
# bits 63:48 must be copies of bit 47. 0x0000800000000000 has bit 47 set but upper
# bits zero, so it is non-canonical and any access faults with #GP (reference rule 7).
# The assembler accepts this; the fault happens at runtime.
	.text
	.globl	bad_canonical
bad_canonical:
	movabsq	$0x0000800000000000, %rax
	movq	(%rax), %rcx
	ret
