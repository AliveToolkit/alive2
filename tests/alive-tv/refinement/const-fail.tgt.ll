@x = internal constant i32 1
@y = internal constant i32 0

define void @f() {
  %a = load i32, ptr @x
  %b = load i32, ptr @y
  ret void
}
