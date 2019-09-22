define i8 @free_null() {
  call void @free(i8* null)
  ret i8 1
}

declare void @free(i8*)

; ERROR: Value mismatch
