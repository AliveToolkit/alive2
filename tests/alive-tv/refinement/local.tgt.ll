define void @f() {
  %p = alloca i32
  store i32 1, ptr %p
  ret void
}
