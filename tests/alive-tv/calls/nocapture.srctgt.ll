declare void @f_nocapture(ptr captures(none) %p)
declare ptr @g()

define i8 @src() {
  %p = alloca i8
  store i8 1, ptr %p
  call void @f_nocapture(ptr %p)
  %q = call ptr @g()
  store i8 2, ptr %q
  %v = load i8, ptr %p
  ret i8 %v
}

define i8 @tgt() {
  %p = alloca i8
  store i8 1, ptr %p
  call void @f_nocapture(ptr %p)
  %q = call ptr @g()
  store i8 2, ptr %q
  ret i8 1
}
