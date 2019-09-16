define i8 @free_ub_doublefree() {
  %ptr = call noalias i8* @malloc(i64 4)
  call void @free(i8* %ptr)
  ret i8 1
}

declare noalias i8* @malloc(i64)
declare void @free(i8*)

; ERROR: Source is more defined than target
