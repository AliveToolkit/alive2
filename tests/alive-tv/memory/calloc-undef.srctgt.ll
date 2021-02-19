define void @src(i1 %cond) {
  br i1 %cond, label %A, label %B
A:
  %ptr = call i8* @calloc(i64 undef, i64 0)
  ret void
B:
  %ptr2 = call i8* @calloc(i64 0, i64 undef)
  ret void
}

define void @tgt(i1 %cond) {
  unreachable
}

declare i8* @calloc(i64, i64)
