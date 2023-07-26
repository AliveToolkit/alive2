define void @f(ptr %p) {
  store i64 0, ptr %p
  ret void
}

define void @f2(ptr %p) {
  store ptr null, ptr %p, align 4
  ret void
}
