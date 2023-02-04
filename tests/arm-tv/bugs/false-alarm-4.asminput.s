	.text
	.file	"reduced.ll"
	.globl	f                               // -- Begin function f
	.p2align	2
	.type	f,@function
f:                                      // @f
	.cfi_startproc
// %bb.0:
	mov	w8, wzr
	cmn	w8, #1
	ccmn	w0, #1, #8, vc
	cset	w0, lt
	ret
.Lfunc_end0:
	.size	f, .Lfunc_end0-f
	.cfi_endproc
                                        // -- End function
	.section	".note.GNU-stack","",@progbits
