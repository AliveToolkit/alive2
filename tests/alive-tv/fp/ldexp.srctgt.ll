declare float @llvm.ldexp.f32.i32(float, i32)

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
