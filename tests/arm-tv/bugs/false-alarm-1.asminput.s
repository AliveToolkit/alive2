	.text
	.file	"false-alarm-1.asminput.ll"
	.globl	f
	.p2align	2
	.type	f,@function
f:
	.cfi_startproc
	sxtb	w9, w0
	mov	x8, #-196
	cmn	w9, #13
	csinv	x8, x8, xzr, lt
	cmp	x8, #0
	cset	w8, ge
	cmn	w9, #13
	csinc	w8, w8, wzr, ge
	sbfx	x0, x8, #0, #1
	ret
.Lfunc_end0:
	.size	f, .Lfunc_end0-f
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
