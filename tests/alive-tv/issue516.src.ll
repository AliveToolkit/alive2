declare i32 @f()

define void @src() {
  call i32 @f()
  ret void
}
