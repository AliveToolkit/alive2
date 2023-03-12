	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 13, 0
	.globl	"_store-pre-indexed-doubleword2" ; -- Begin function store-pre-indexed-doubleword2
	.p2align	2
"_store-pre-indexed-doubleword2":       ; @store-pre-indexed-doubleword2
	.cfi_startproc
; %bb.0:
	ldr	x8, [x0]
	str	xzr, [x8]
	ret
	.cfi_endproc
                                        ; -- End function
.subsections_via_symbols
