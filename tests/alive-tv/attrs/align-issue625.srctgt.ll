define align 2 i64* @src() {
  %p = alloca i64, align 4
  store i64 1, i64* %p, align 4
  ret i64* %p
}

define align 2 i64* @tgt() {
  %p = alloca i64, align 4
  ret i64* %p
}
