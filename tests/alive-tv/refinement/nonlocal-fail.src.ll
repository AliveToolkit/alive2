define void @f(ptr %p, ptr %q) {
  store i32 10, ptr %p
  store i32 20, ptr %q
  ret void
}

; ERROR: Mismatch in memory
