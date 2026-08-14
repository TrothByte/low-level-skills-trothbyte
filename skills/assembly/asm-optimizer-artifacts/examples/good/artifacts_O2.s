	.file	"artifacts.c"
	.text
	.p2align 4
	.globl	tail
	.def	tail;	.scl	2;	.type	32;	.endef
	.seh_proc	tail
tail:
	.seh_endprologue
	jmp	helper
	.seh_endproc
	.p2align 4
	.globl	mul3
	.def	mul3;	.scl	2;	.type	32;	.endef
	.seh_proc	mul3
mul3:
	.seh_endprologue
	leal	(%rcx,%rcx,2), %eax
	ret
	.seh_endproc
	.p2align 4
	.globl	fold
	.def	fold;	.scl	2;	.type	32;	.endef
	.seh_proc	fold
fold:
	.seh_endprologue
	movl	$14, %eax
	ret
	.seh_endproc
	.p2align 4
	.globl	dce
	.def	dce;	.scl	2;	.type	32;	.endef
	.seh_proc	dce
dce:
	.seh_endprologue
	leal	1(%rcx), %eax
	ret
	.seh_endproc
	.p2align 4
	.globl	caller
	.def	caller;	.scl	2;	.type	32;	.endef
	.seh_proc	caller
caller:
	.seh_endprologue
	leal	2(%rcx,%rcx), %eax
	ret
	.seh_endproc
	.p2align 4
	.globl	pic_read
	.def	pic_read;	.scl	2;	.type	32;	.endef
	.seh_proc	pic_read
pic_read:
	.seh_endprologue
	movq	.refptr.g(%rip), %rax
	movl	(%rax), %eax
	ret
	.seh_endproc
	.ident	"GCC: (Rev5, Built by MSYS2 project) 16.1.0"
	.def	helper;	.scl	2;	.type	32;	.endef
	.section	.rdata$.refptr.g, "dr"
	.p2align	3, 0
	.globl	.refptr.g
	.linkonce	discard
.refptr.g:
	.quad	g
