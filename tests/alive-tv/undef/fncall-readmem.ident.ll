@x = global i32 0
declare void @g() noreturn

define void @f(i32 %v) {
  store i32 %v, ptr @x
  call void @g()
  unreachable
}
