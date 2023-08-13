@x = global i32 0

define void @f() {
  store i32 20, ptr @x
  ret void
}
