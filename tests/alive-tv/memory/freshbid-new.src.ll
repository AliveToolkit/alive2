define i8 @freshbid_new(ptr %ptr0) {
  %ptr = call noalias ptr @_Znwm(i64 1)
  store i8 10, ptr %ptr
  store i8 20, ptr %ptr0
  %v = load i8, ptr %ptr
  ret i8 %v
}

declare ptr @_Znwm(i64)
