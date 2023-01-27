	.text
	.file	"ccmp_spams_output_reg.asminput.ll"
	.globl	test18                          // -- Begin function test18
	.p2align	2
	.type	test18,@function
test18:                                 // @test18
	.cfi_startproc
// %bb.0:
	mov	w8, #50
	cmp	w0, #99
	ccmp	w0, w8, #8, le
	ret
.Lfunc_end0:
	.size	test18, .Lfunc_end0-test18
	.cfi_endproc
                                        // -- End function
	.section	".note.GNU-stack","",@progbits
