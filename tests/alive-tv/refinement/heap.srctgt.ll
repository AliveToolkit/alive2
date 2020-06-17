declare i8* @malloc(i64)

define i8* @src(i64 %x) {
  %p = call i8* @malloc(i64 8)
  %p8 = bitcast i8* %p to i64*
  load i64, i64* %p8 ; guarantees dereferenceability
  ret i8* %p
}

define i8* @tgt(i64 %x) {
  %p = call i8* @malloc(i64 8)
  %p8 = bitcast i8* %p to i64*
  store i64 %x, i64* %p8
  ret i8* %p
}
