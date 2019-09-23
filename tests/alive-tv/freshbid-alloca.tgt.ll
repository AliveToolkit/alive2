define i8 @freshbid_alloca(i8* %ptr0) {
  %ptr = alloca i8
  store i8 10, i8* %ptr
  store i8 20, i8* %ptr0
  %v = load i8, i8* %ptr
  ret i8 10
}
