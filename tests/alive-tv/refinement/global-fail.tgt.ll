@x = global i32 0

define void @f() {
  store i32 20, i32* @x
  ret void
}
