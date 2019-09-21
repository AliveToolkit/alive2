define i8 @free_noub_gep() {
  %ptr0 = call noalias i8* @malloc(i64 4)
  %ptr = getelementptr i8, i8* %ptr0, i32 0
  call void @free(i8* %ptr)
  ret i8 1
}

declare noalias i8* @malloc(i64)
declare void @free(i8*)
