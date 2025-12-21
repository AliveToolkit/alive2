define float @src() {
  %nan = fdiv float 0.0, 0.0
  %r = call float @llvm.ldexp.f32.i32(float %nan, i32 3)
  ret float %r
}

define float @tgt() {
  %nan = fdiv float 0.0, 0.0
  ret float %nan
}

define float @src1() {
  %inf = fdiv float 1.0, 0.0
  %r = call float @llvm.ldexp.f32.i32(float %inf, i32 -7)
  ret float %r
}

define float @tgt1() {
  %inf = fdiv float 1.0, 0.0
  ret float %inf
}

define float @src2() {
  %inf = fdiv float -1.0, 0.0
  %r = call float @llvm.ldexp.f32.i32(float %inf, i32 11)
  ret float %r
}

define float @tgt2() {
  %inf = fdiv float -1.0, 0.0
  ret float %inf
}

define float @src3() {
  %r = call float @llvm.ldexp.f32.i32(float 0.0, i32 9)
  ret float %r
}

define float @tgt3() {
  ret float 0.0
}

define float @src4() {
  %r = call float @llvm.ldexp.f32.i32(float -0.0, i32 -9)
  ret float %r
}

define float @tgt4() {
  ret float -0.0
}

define float @src5() {
  %r = call float @llvm.ldexp.f32.i32(float -1.0, i32 -200)
  ret float %r
}

define float @tgt5() {
  ret float -0.0
}

define float @src6() {
  %r = call float @llvm.ldexp.f32.i32(float -1.0, i32 200)
  ret float %r
}

define float @tgt6() {
  %inf = fdiv float -1.0, 0.0
  ret float %inf
}

define <3 x float> @src7() {
  %r = call <3 x float> @llvm.ldexp.v3f32.v3i32(<3 x float> <float -0.0, float 7.0, float 101.0>, <3 x i32> <i32 -100, i32 3, i32 1>)
  ret <3 x float> %r
}

define <3 x float> @tgt7() {
  ret <3 x float> <float -0.0, float 56.0, float 202.0>
}

define float @src8() {
  %r1 = call float @llvm.ldexp.f32.i32(float 11.0, i32 2)
  %r2 = call float @llvm.ldexp.f32.i32(float %r1, i32 3)
  ret float %r2
}

define float @tgt8() {
  %r = call float @llvm.ldexp.f32.i32(float 11.0, i32 5)
  ret float %r
}

define float @src9() {
  %r = call float @llvm.ldexp.f32.i7(float 1.25, i7 3)
  ret float %r
}

define float @tgt9() {
  ret float 10.0
}

define half @src10() {
  %r = call half @llvm.ldexp.f16.i7(half 0.5, i7 1)
  ret half %r
}

define half @tgt10() {
  ret half 1.0
}

define half @src11() {
  %r = call half @llvm.ldexp.f16.i13(half 1.0, i13 -30)
  ret half %r
}

define half @tgt11() {
  ret half 0.0
}

define half @src12() {
  %r = call half @llvm.ldexp.f16.i13(half -1.0, i13 30)
  ret half %r
}

define half @tgt12() {
  %inf = fdiv half -1.0, 0.0
  ret half %inf
}

define double @src13() {
  %r = call double @llvm.ldexp.f64.i7(double 3.25, i7 2)
  ret double %r
}

define double @tgt13() {
  ret double 13.0
}

define double @src14() {
  %r = call double @llvm.ldexp.f64.i13(double 8.0, i13 -3)
  ret double %r
}

define double @tgt14() {
  ret double 1.0
}

; Currently times out
; define float @src15(float %x) {
;   %r1 = call float @llvm.ldexp.f32.i32(float %x, i32 2)
;   %r2 = call float @llvm.ldexp.f32.i32(float %r1, i32 3)
;   ret float %r2
; }

; define float @tgt15(float %x) {
;   %r = call float @llvm.ldexp.f32.i32(float %x, i32 5)
;   ret float %r
; }

define <5 x double> @src_fail() {
  %r = call <5 x double> @llvm.ldexp.v5f64.v5i8(<5 x double> <double 1.0, double 1.0, double 1.0, double 1.0, double 1.0>, <5 x i8> <i8 -2, i8 -1, i8 0, i8 1, i8 2>)
  ret <5 x double> %r
}

define <5 x double> @tgt_fail() {
  ret <5 x double> <double 0.25, double 0.5, double 1.0, double 2.0, double 4.0>
}
