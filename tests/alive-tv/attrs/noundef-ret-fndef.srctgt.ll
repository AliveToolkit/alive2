declare noundef i1 @f()

define i32 @src() {
  %cond = call i1 @f()
  %f = freeze i1 %cond
  br i1 %f, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}

define i32 @tgt() {
  %cond = call i1 @f()
  br i1 %cond, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}
