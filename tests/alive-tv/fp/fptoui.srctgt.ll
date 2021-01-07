; https://reviews.llvm.org/D89697

define i32 @src(float %f) {
  %r = fptoui float %f to i32
  ret i32 %r
}

define i32 @tgt(float %f) {
  %reduced = fsub float %f, 2147483648.0
  %reduced_int = fptosi float %reduced to i32
  %big = or i32 %reduced_int, 2147483648
  %small = fptosi float %f to i32
  %is_big = fcmp oge float %f, 2147483648.0
  %r = select i1 %is_big, i32 %big, i32 %small
  ret i32 %r
}
