declare void @f()

define i8 @src() {
  call void @f() noreturn
  ret i8 0
}

define i8 @tgt() {
  call void @f()
  ret i8 0
}
