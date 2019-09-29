define i8 @free_ub_useafterfree() {
  %ptr = call noalias i8* @malloc(i64 4)
  store i8 10, i8* %ptr
  call void @free(i8* %ptr)
  %v = load i8, i8* %ptr
  ret i8 %v
}

declare noalias i8* @malloc(i64)
declare void @free(i8*)
