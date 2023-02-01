	.text
	.file	"fuzz"
	.globl	f                               // -- Begin function f
	.p2align	2
	.type	f,@function
f:                                      // @f
	.cfi_startproc
// %bb.0:
	and	w8, w7, #0x3
	sxth	w9, w4
	add	x10, sp, #32
	smull	x8, w8, w9
	lsr	x9, x8, #32
	extr	w8, w9, w8, #8
	and	x9, x6, #0x1ff
	sxth	x8, w8
	cmp	x8, x9
	csel	x8, x8, x9, lt
	ldr	x9, [sp, #48]
	tst	w8, #0x1
	mov	x8, sp
	csel	x8, x10, x8, ne
	sxth	x10, w1
	sbfx	x9, x9, #0, #55
	ldrsb	w8, [x8]
	asr	x9, x9, x10
	lsl	w9, w9, #10
	cmp	w8, w9, asr #10
	cset	w0, gt
	ret
.Lfunc_end0:
	.size	f, .Lfunc_end0-f
	.cfi_endproc
                                        // -- End function
	.section	".note.GNU-stack","",@progbits
