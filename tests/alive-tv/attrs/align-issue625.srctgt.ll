define align 2 ptr @src() {
  %p = alloca i64, align 4
  store i64 1, ptr %p, align 4
  ret ptr %p
}

define align 2 ptr @tgt() {
  %p = alloca i64, align 4
  ret ptr %p
}
