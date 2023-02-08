	.text
	.file	"reduced.ll"
	.globl	f                               // -- Begin function f
	.p2align	2
	.type	f,@function
f:                                      // @f
	.cfi_startproc
// %bb.0:
	sxtb	w8, w0
	cmp	w8, #0
	ccmn	w8, #16, #0, lt
	cset	w0, lt
	ret
.Lfunc_end0:
	.size	f, .Lfunc_end0-f
	.cfi_endproc
                                        // -- End function
	.section	".note.GNU-stack","",@progbits
