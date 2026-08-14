	.file	"artifacts.c"
	.text
	.globl	tail
	.def	tail;	.scl	2;	.type	32;	.endef
	.seh_proc	tail
tail:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	subq	$32, %rsp
	.seh_stackalloc	32
	.seh_endprologue
	movl	%ecx, 16(%rbp)
	movl	16(%rbp), %eax
	movl	%eax, %ecx
	call	helper
	addq	$32, %rsp
	popq	%rbp
	ret
	.seh_endproc
	.globl	mul3
	.def	mul3;	.scl	2;	.type	32;	.endef
	.seh_proc	mul3
mul3:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	.seh_endprologue
	movl	%ecx, 16(%rbp)
	movl	16(%rbp), %edx
	movl	%edx, %eax
	addl	%eax, %eax
	addl	%edx, %eax
	popq	%rbp
	ret
	.seh_endproc
	.globl	fold
	.def	fold;	.scl	2;	.type	32;	.endef
	.seh_proc	fold
fold:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	.seh_endprologue
	movl	$14, %eax
	popq	%rbp
	ret
	.seh_endproc
	.globl	dce
	.def	dce;	.scl	2;	.type	32;	.endef
	.seh_proc	dce
dce:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	subq	$16, %rsp
	.seh_stackalloc	16
	.seh_endprologue
	movl	%ecx, 16(%rbp)
	movl	16(%rbp), %eax
	imull	$100, %eax, %eax
	movl	%eax, -4(%rbp)
	movl	16(%rbp), %eax
	addl	$1, %eax
	addq	$16, %rsp
	popq	%rbp
	ret
	.seh_endproc
	.def	inline_me;	.scl	3;	.type	32;	.endef
	.seh_proc	inline_me
inline_me:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	.seh_endprologue
	movl	%ecx, 16(%rbp)
	movl	16(%rbp), %eax
	addl	$1, %eax
	popq	%rbp
	ret
	.seh_endproc
	.globl	caller
	.def	caller;	.scl	2;	.type	32;	.endef
	.seh_proc	caller
caller:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	subq	$32, %rsp
	.seh_stackalloc	32
	.seh_endprologue
	movl	%ecx, 16(%rbp)
	movl	16(%rbp), %eax
	movl	%eax, %ecx
	call	inline_me
	addl	%eax, %eax
	addq	$32, %rsp
	popq	%rbp
	ret
	.seh_endproc
	.globl	pic_read
	.def	pic_read;	.scl	2;	.type	32;	.endef
	.seh_proc	pic_read
pic_read:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	.seh_endprologue
	movq	.refptr.g(%rip), %rax
	movl	(%rax), %eax
	popq	%rbp
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
