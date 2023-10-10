@glb = global i8 0

define i8 @glb_malloc_noalias() {
  %ptr = call ptr @malloc(i64 1)
  store i8 10, ptr %ptr
  store i8 20, ptr @glb
  %v = load i8, ptr %ptr
  ret i8 %v
}

declare noalias ptr @malloc(i64)
