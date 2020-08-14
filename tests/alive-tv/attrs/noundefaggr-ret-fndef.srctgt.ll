declare noundef {i8, i32} @f()

define i32 @src() {
  %v = call {i8, i32} @f()
  %w = extractvalue {i8, i32} %v, 0
  %cond = icmp eq i8 %w, 10
  %f = freeze i1 %cond
  br i1 %f, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}

define i32 @tgt() {
  %v = call {i8, i32} @f()
  %w = extractvalue {i8, i32} %v, 0
  %cond = icmp eq i8 %w, 10
  br i1 %cond, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}
