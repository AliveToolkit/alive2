define i8* @malloc_null() {
  %ptr = call noalias i8* @malloc(i64 1)
  store i8 10, i8* %ptr
  ret i8* %ptr
}

declare noalias i8* @malloc(i64)
