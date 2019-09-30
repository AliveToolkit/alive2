@glb = global i8 0

define i8 @glb_malloc_noalias() {
  %ptr = call noalias i8* @malloc(i64 1)
  store i8 10, i8* %ptr
  store i8 20, i8* @glb
  %v = load i8, i8* %ptr
  ret i8 10
}

declare noalias i8* @malloc(i64)
