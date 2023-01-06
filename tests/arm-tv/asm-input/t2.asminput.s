	.text
	.file	"t2.aarch64.ll"
	.globl	f
	.p2align	2
	.type	f,@function
f:
	.cfi_startproc
	add	w0, w0, w1
	ret
.Lfunc_end0:
	.size	f, .Lfunc_end0-f
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
