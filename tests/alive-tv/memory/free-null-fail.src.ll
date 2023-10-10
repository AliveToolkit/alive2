define i8 @free_null() {
  call void @free(ptr null)
  ret i8 1
}

declare void @free(ptr)

; ERROR: Value mismatch
