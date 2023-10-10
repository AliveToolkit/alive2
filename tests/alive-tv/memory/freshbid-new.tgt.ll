define i8 @freshbid_new(ptr %ptr0) {
  %ptr = call noalias ptr @_Znwm(i64 1)
  store i8 10, ptr %ptr
  store i8 20, ptr %ptr0
  ret i8 10
}

declare ptr @_Znwm(i64)
