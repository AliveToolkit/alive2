	.text
	.file	"fuzz"
	.globl	f                               // -- Begin function f
	.p2align	2
	.type	f,@function
f:                                      // @f
	.cfi_startproc
// %bb.0:
	mov	w8, #-1
	mov	w12, #1
	sdiv	w9, w8, w0
	subs	w8, w8, w1
	csel	w8, wzr, w8, lo
	smull	x10, w8, w1
	lsr	x11, x10, #32
	mul	w9, w9, w0
	extr	w10, w11, w10, #16
	mov	w11, #-17
	bics	wzr, w12, w9
	mvn	w9, w9
	csel	w11, w0, w11, ne
	cmp	w8, #0
	cinc	w8, w10, gt
	cmp	w11, w8
	ccmn	w9, #1, #2, lt
	cset	w0, lo
	ret
.Lfunc_end0:
	.size	f, .Lfunc_end0-f
	.cfi_endproc
                                        // -- End function
	.section	".note.GNU-stack","",@progbits
