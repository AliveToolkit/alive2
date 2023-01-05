	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 13, 0
	.globl	_f                              ; -- Begin function f
	.p2align	2
_f:                                     ; @f
	.cfi_startproc
; %bb.0:
	add	w0, w0, w1
	ret
	.cfi_endproc
                                        ; -- End function
.subsections_via_symbols
