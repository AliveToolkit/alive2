	.text
	.file	"i8_signext.aarch64.ll"
	.globl	sext_arg_i8
	.p2align	2
	.type	sext_arg_i8,@function
sext_arg_i8:
	.cfi_startproc
	sxtb	w0, w0
	ret
.Lfunc_end0:
	.size	sext_arg_i8, .Lfunc_end0-sext_arg_i8
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
