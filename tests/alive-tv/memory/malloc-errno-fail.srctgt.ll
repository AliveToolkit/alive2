define i32 @src(ptr %p, i64 %sz) {
  store i32 0, ptr %p
  call ptr @malloc(i64 %sz)
  %v = load i32, ptr %p
  ret i32 %v
}

define i32 @tgt(ptr %p, i64 %sz) {
  store i32 0, ptr %p
  ret i32 0
}

declare noalias ptr @malloc(i64) memory(inaccessiblemem: readwrite, errnomem: write)

; ERROR: Mismatch in errno at return
