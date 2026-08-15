# GOOD: every mnemonic here exists in the Intel SDM, the file assembles, and
# objdump confirms the intended encodings (recorded in evals/README.md).
	.text
	.globl	verify_imul38
verify_imul38:
	imull	$38, %eax, %eax		# 6b c0 26 — imul imm8 form, NOT nulled
	ret

	.globl	verify_att_order
verify_att_order:
	movl	%ebx, %eax		# AT&T: source %ebx, destination %eax
	movl	$0, -4(%rbp)		# '$' makes 0 an immediate, not a memory ref
	ret

	.globl	verify_esp_top
verify_esp_top:
	movl	(%rsp), %eax		# reads the slot at the top of stack
	ret
