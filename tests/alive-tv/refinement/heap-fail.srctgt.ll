declare i8* @malloc(i64)

define i8* @src() {
  %p = call i8* @malloc(i64 8)
  %p8 = bitcast i8* %p to i64*
  store i64 1, i64* %p8
  ret i8* %p
}

define i8* @tgt() {
  %p = call i8* @malloc(i64 8)
  %p8 = bitcast i8* %p to i64*
  store i64 2, i64* %p8
  ret i8* %p
}

; ERROR: Value mismatch
