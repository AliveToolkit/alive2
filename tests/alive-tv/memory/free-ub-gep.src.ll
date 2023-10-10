define i8 @free_ub_gep() {
  %ptr0 = call ptr @malloc(i64 4)
  %ptr = getelementptr i8, ptr %ptr0, i32 1
  call void @free(ptr %ptr)
  ret i8 0
}

declare noalias ptr @malloc(i64)
declare void @free(ptr)
