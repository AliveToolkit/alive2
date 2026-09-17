; llvm/test/Transforms/InstCombine/is_fpclass.ll,
; @test_class_is_p0_n0_psub_nsub_f32_dapz
;
; Under an input denormal mode of positivezero, a subnormal operand of the
; fcmp is treated as +0.0, so "x is zero or subnormal" is exactly "x == 0".
; llvm.is.fpclass itself classifies the unflushed value.

define i1 @src(float %x) denormal_fpenv(ieee|positivezero) {
  %val = call i1 @llvm.is.fpclass.f32(float %x, i32 240)
  ret i1 %val
}

define i1 @tgt(float %x) denormal_fpenv(ieee|positivezero) {
  %val = fcmp oeq float %x, 0.000000e+00
  ret i1 %val
}

declare i1 @llvm.is.fpclass.f32(float, i32 immarg)
