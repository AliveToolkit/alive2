	.text
	.file	"fuzz"
	.globl	f                               // -- Begin function f
	.p2align	2
	.type	f,@function
f:                                      // @f
	.cfi_startproc
// %bb.0:
	ldr	w8, [sp, #8]
	and	x9, x1, #0xffffffff
	tst	w2, #0x1
	add	x10, sp, #32
	add	x11, sp, #24
	mov	w12, #67108863
	umull	x8, w9, w8
	csel	x9, x11, x10, ne
	ldr	w11, [sp, #16]
	lsr	x10, x8, #32
	ldr	w9, [x9]
	cmp	w10, w12
	extr	w8, w10, w8, #26
	csinv	w8, w8, wzr, ls
	cmp	w11, w9
	csetm	w9, ne
	tst	w2, #0x1
	csel	w0, w8, w9, ne
	ret
.Lfunc_end0:
	.size	f, .Lfunc_end0-f
	.cfi_endproc
                                        // -- End function
	.section	".note.GNU-stack","",@progbits
