define i8 @free_ub_useafterfree() {
  %ptr = call noalias ptr @malloc(i64 4)
  store i8 10, ptr %ptr
  call void @free(ptr %ptr)
  %v = load i8, ptr %ptr
  ret i8 %v
}

declare noalias ptr @malloc(i64)
declare void @free(ptr)
