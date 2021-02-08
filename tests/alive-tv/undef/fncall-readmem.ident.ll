@x = global i32 0
declare void @g() noreturn

define void @f(i32 %v) {
  store i32 %v, i32* @x
  call void @g()
  unreachable
}
