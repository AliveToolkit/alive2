define i8 @free_null() {
  call void @free(i8* null)
  ret i8 2
}

declare void @free(i8*)
