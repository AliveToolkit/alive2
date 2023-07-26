define void @f() {
  %p = alloca i32
  store i32 0, ptr %p
  ret void
}
