define void @f(i32* %p, i32* %q) {
  store i32 10, i32* %p
  store i32 20, i32* %q
  ret void
}

; ERROR: Mismatch in memory
