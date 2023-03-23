	.text
	.file	"test-729604639.ll"
	.globl	i32_m1
	.p2align	2
	.type	i32_m1,@function
i32_m1:
	.cfi_startproc
	str	w1, [x0, #-1]!
	ret
.Lfunc_end0:
	.size	i32_m1, .Lfunc_end0-i32_m1
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits