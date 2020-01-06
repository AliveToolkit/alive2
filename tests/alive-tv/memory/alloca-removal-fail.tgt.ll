define i8* @f_observed(i64 %size) {
entry:
  ret i8* null
}

declare dso_local noalias i8* @malloc(i64)
