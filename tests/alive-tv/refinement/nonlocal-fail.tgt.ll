define void @f(ptr %p, ptr %q) {
  store i32 20, ptr %q
  store i32 10, ptr %p
  ret void
}
