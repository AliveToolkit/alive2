define i8 @free_ub_useafterfree_2(i8* %ptr) {
  call void @free(i8* %ptr)
  %v = load i8, i8* %ptr
  ret i8 %v
}

declare noalias i8* @malloc(i64)
declare void @free(i8*)
