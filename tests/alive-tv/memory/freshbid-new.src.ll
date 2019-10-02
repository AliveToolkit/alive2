define i8 @freshbid_new(i8* %ptr0) {
  %ptr = call noalias i8* @_Znwm(i64 1)
  store i8 10, i8* %ptr
  store i8 20, i8* %ptr0
  %v = load i8, i8* %ptr
  ret i8 %v
}

declare i8* @_Znwm(i64)
