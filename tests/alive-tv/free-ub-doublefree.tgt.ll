define i8 @free_ub_doublefree() {
  ret i8 2
}

declare noalias i8* @malloc(i64)
declare void @free(i8*)
