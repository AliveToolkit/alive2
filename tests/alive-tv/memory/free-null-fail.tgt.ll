define i8 @free_null() {
  call void @free(ptr null)
  ret i8 2
}

declare void @free(ptr)
