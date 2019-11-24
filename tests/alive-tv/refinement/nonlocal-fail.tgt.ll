define void @f(i32* %p, i32* %q) {
  store i32 20, i32* %q
  store i32 10, i32* %p
  ret void
}
