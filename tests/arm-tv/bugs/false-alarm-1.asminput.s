	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 13, 0
	.globl	_f                              ; -- Begin function f
	.p2align	2
_f:                                     ; @f
	.cfi_startproc
; %bb.0:
	sxtb	w9, w0
	mov	x8, #-196
	cmn	w9, #14
	csinv	x8, x8, xzr, le
	cmp	x8, #0
	ccmn	w9, #13, #8, lt
	cset	w8, lt
	sbfx	x0, x8, #0, #1
	ret
	.cfi_endproc
                                        ; -- End function
.subsections_via_symbols
