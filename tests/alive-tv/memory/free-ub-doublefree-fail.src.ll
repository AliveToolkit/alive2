define i8 @free_ub_doublefree() {
  %ptr = call noalias ptr @malloc(i64 4)
  call void @free(ptr %ptr)
  ret i8 1
}

declare noalias ptr @malloc(i64)
declare void @free(ptr)

; ERROR: Source is more defined than target
