# BAD 1: scale factor 3 is illegal — scale must be 1, 2, 4, or 8.
# `as` error: "expecting scale factor of 1, 2, 4, or 8: got `3'".
	.text
	.globl	bad_scale
bad_scale:
	movq	(%rax, %rbx, 3), %rcx
	ret

# BAD 2: %rsp cannot be an index register — the SIB index field 100b means "no index",
# so the encoder cannot represent it.
# `as` error: "`(%rax,%rsp,2)' is not a valid base/index expression".
	.text
	.globl	bad_rsp_index
bad_rsp_index:
	movq	(%rax, %rsp, 2), %rcx
	ret
