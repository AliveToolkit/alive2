define i8 @freshbid_alloca(ptr %pptr) {
  %ptr = alloca i8
  %ptr0 = load ptr, ptr %pptr
  store i8 10, ptr %ptr
  store i8 20, ptr %ptr0
  %v = load i8, ptr %ptr
  ret i8 %v
}
