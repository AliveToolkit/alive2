declare void @f() noreturn

define void @src(i1 %cond) {
  br i1 %cond, label %A, label %B
A:
  unreachable
B:
  call void @f()
  ret void
}

define void @tgt(i1 %cond) {
  call void @f()
  ret void
}
