	.text
	.file	"fuzz"
	.globl	f                               // -- Begin function f
	.p2align	2
	.type	f,@function
f:                                      // @f
	.cfi_startproc
// %bb.0:
	lsl	w9, w0, #24
	sxtb	w8, w0
	lsl	w11, w9, w0
	ands	w10, w0, #0x1
	and	w12, w0, #0xff
	lsr	w13, w11, w0
	csel	w8, w8, w12, ne
	cmp	w9, w13
	csinv	w9, w11, wzr, eq
	cmp	w8, w12
	lsr	w9, w9, #24
	and	w9, w9, #0xff
	csel	w8, w9, w10, hi
	cmp	w12, w8
	ccmn	w8, #2, #4, hs
	cset	w0, eq
	ret
.Lfunc_end0:
	.size	f, .Lfunc_end0-f
	.cfi_endproc
                                        // -- End function
	.section	".note.GNU-stack","",@progbits
