declare void @f() noreturn

define void @src(i1 %cond) {
  br i1 %cond, label %A, label %B
A:
  udiv i32 1, 0 ; cannot use unreachable due to a bug; it does not call addReturn
  ret void
B:
  call void @f()
  ret void
}

define void @tgt(i1 %cond) {
  call void @f()
  ret void
}
