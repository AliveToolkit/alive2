	.text
	.file	"fuzz"
	.globl	f                               // -- Begin function f
	.p2align	2
	.type	f,@function
f:                                      // @f
	.cfi_startproc
// %bb.0:
	ldr	w8, [sp, #56]
	add	x10, sp, #24
	ldrb	w9, [sp, #32]
	add	x11, sp, #16
	tst	w8, #0x1
	csel	x8, x11, x10, ne
	mul	w9, w9, w4
	sxtb	w10, w4
	sbfx	w9, w9, #0, #1
	ldrsb	w8, [x8]
	cmp	w10, w9
	cset	w9, le
	cmp	w8, w9
	ldr	x8, [sp, #40]
	cset	w9, gt
	sbfx	x9, x9, #0, #1
	udiv	x10, x9, x8
	msub	w0, w10, w8, w9
	ret
.Lfunc_end0:
	.size	f, .Lfunc_end0-f
	.cfi_endproc
                                        // -- End function
	.section	".note.GNU-stack","",@progbits
