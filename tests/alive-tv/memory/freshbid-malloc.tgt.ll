define i8 @freshbid_malloc(i8* %ptr0) {
  %ptr = call noalias i8* @malloc(i64 1)
  store i8 10, i8* %ptr
  store i8 20, i8* %ptr0
  %v = load i8, i8* %ptr
  ret i8 10
}

declare noalias i8* @malloc(i64)
declare void @free(i8*)
