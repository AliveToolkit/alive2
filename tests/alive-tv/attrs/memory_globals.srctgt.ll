; ERROR: Mismatch in memory

@x = external global i32, align 4

define void @src() memory(readwrite, argmem:none) {
  store i32 0, ptr @x, align 4
  ret void
}

define void @tgt() memory(readwrite, argmem:none) {
  store i32 1, ptr @x, align 4
  ret void
}
