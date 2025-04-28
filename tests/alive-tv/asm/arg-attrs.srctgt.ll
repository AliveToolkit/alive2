; TEST-ARGS: -tgt-is-asm

define <4 x float> @src(<2 x float> nofpclass(nan) %0) {
  %2 = shufflevector <2 x float> zeroinitializer, <2 x float> %0, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  ret <4 x float> %2
}

define <4 x float> @tgt(<2 x float> nofpclass(nan) %0) {
  %a0_99 = bitcast <2 x float> %0 to i64
  %a4_10 = insertelement <2 x i64> <i64 0, i64 poison>, i64 %a0_99, i64 1
  %r = bitcast <2 x i64> %a4_10 to <4 x float>
  ret <4 x float> %r
}
