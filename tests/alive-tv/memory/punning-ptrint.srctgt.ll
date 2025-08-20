define i64 @src1(ptr %pptr) {
  %ptr = alloca i8
  store ptr %ptr, ptr %pptr
  %v = load i64, ptr %pptr
  ret i64 %v
}

define i64 @tgt1(ptr %pptr) {
  %ptr = alloca i8
  store ptr %ptr, ptr %pptr
  %r = ptrtoaddr ptr %ptr to i64
  ret i64 %r
}

define i16 @src2(ptr %pptr) {
  %ptr = alloca i8
  store ptr %ptr, ptr %pptr
  %v = load i16, ptr %pptr
  ret i16 %v
}

define i16 @tgt2(ptr %pptr) {
  %ptr = alloca i8
  store ptr %ptr, ptr %pptr
  %addr = ptrtoaddr ptr %ptr to i64
  %r = trunc i64 %addr to i16
  ret i16 %r
}


@glb = global i8 0, align 256

define i8 @src3(ptr %pptr) {
  store ptr @glb, ptr %pptr
  %v = load i8, ptr %pptr
  ret i8 %v
}

define i8 @tgt3(ptr %pptr) {
  store ptr @glb, ptr %pptr
  ret i8 0
}
