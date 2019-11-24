@x = global i32 0
@y = global i32 0
@z = global i32 0
@w = global i32 0

define void @f() {
  store i32 40, i32* @w
  store i32 30, i32* @z
  store i32 20, i32* @y
  store i32 10, i32* @x
  ret void
}
