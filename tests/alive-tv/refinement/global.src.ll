@x = global i32 0
@y = global i32 0
@z = global i32 0
@w = global i32 0

define void @f() {
  store i32 10, ptr @x
  store i32 20, ptr @y
  store i32 30, ptr @z
  store i32 40, ptr @w
  ret void
}
