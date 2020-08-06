declare void @f(i1 noundef)

define i32 @src(i1 %cond) {
  call void @f(i1 %cond)
  %f = freeze i1 %cond
  br i1 %f, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}

define i32 @tgt(i1 %cond) {
  call void @f(i1 %cond)
  br i1 %cond, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}
