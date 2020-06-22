; This test fails if Alloca::toSMT does not have sz's sign check
declare void @free(i8*)

define void @src(i32 %x) {
  %conv = sext i32 %x to i64
  %mul = mul nsw i64 %conv, 4
  %xx = alloca i8, i64 %mul, align 16
  %y= bitcast i8* %xx to float*
  store float 0.0, float* %y
  ret void
}

define void @tgt(i32 %x) {
  %conv = sext i32 %x to i64
  %y = alloca float, i64 %conv, align 16
  store float 0.0, float* %y
  ret void
}
