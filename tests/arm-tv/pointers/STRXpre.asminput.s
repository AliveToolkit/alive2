	.text
	.file	"test-956546981.ll"
	.globl	trunc_
	.p2align	2
	.type	trunc_,@function
trunc_:
	and	w0, w3, #0xff
	str	x3, [sp, #-16]!
	str	w3, [sp, #8]
	strh	w3, [sp, #12]
	strb	w3, [sp, #15]
	add	sp, sp, #16
	ret
.Lfunc_end0:
	.size	trunc_, .Lfunc_end0-trunc_

	.section	".note.GNU-stack","",@progbits
