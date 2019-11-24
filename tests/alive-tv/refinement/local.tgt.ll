define void @f() {
  %p = alloca i32
  store i32 1, i32* %p
  ret void
}
