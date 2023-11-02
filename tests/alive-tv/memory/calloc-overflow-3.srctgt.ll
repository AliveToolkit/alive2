define i8 @src(ptr) {
  %ptr = call ptr @calloc(i64 555555555555555, i64 40000000)
  call void @free(ptr null)
  ret i8 1
}

define i8 @tgt(ptr) {
  ret i8 1
}

declare void @free(ptr)
declare ptr @calloc(i64, i64)
