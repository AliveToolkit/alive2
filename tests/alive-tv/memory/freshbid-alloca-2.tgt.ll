define i8 @freshbid_alloca(ptr %pptr) {
  %ptr0 = load ptr, ptr %pptr
  store i8 20, ptr %ptr0
  ret i8 10
}
