define i32 @src(ptr %p) {
  store i32 0, ptr %p
  call ptr @malloc(i64 -9223372036854775808)
  %v = load i32, ptr %p
  ret i32 %v
}

define i32 @tgt(ptr %p) {
  store i32 0, ptr %p
  ret i32 0
}

declare noalias ptr @malloc(i64) memory(inaccessiblemem: readwrite, errnomem: write)

; ERROR: Mismatch in errno at return
