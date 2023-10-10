define void @f(ptr %p) {
  %v = load i8, ptr %p
  store i8 %v, ptr %p
  ret void
}

define void @f2(ptr %p) {
  %v = load ptr, ptr %p
  store ptr %v, ptr %p
  ret void
}
