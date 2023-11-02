define void @src(i1 %cond) {
  br i1 %cond, label %A, label %B
A:
  %ptr = call ptr @calloc(i64 undef, i64 0)
  ret void
B:
  %ptr2 = call ptr @calloc(i64 0, i64 undef)
  ret void
}

define void @tgt(i1 %cond) {
  unreachable
}

declare ptr @calloc(i64, i64)
