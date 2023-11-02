define i8 @free_ub_useafterfree_2(ptr %ptr) {
  call void @free(ptr %ptr)
  %v = load i8, ptr %ptr
  ret i8 %v
}

declare noalias ptr @malloc(i64)
declare void @free(ptr)
