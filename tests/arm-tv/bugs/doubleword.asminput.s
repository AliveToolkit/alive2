	.text
	.file	"doubleword.asminput.ll"
	.globl	"store-pre-indexed-doubleword2"
	.p2align	2
	.type	"store-pre-indexed-doubleword2",@function
"store-pre-indexed-doubleword2":
	.cfi_startproc
	ldr	x8, [x0]
	str	xzr, [x8]
	ret
.Lfunc_end0:
	.size	"store-pre-indexed-doubleword2", .Lfunc_end0-"store-pre-indexed-doubleword2"
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
