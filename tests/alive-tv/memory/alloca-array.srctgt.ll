define void @src(i32 %x) {
  %conv = sext i32 %x to i64
  %mul = mul nsw i64 %conv, 4
  %xx = alloca i8, i64 %mul, align 16
  store float 0.0, ptr %xx
  ret void
}

define void @tgt(i32 %x) {
  %conv = sext i32 %x to i64
  %y = alloca float, i64 %conv, align 16
  store float 0.0, ptr %y
  ret void
}
