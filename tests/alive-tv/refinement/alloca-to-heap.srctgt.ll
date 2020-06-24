declare i8* @malloc(i64)

define i8* @src() {
 %p = alloca i64
 %p8 = bitcast i64* %p to i8*
 ret i8* %p8
}

define i8* @tgt() {
 %p = call i8* @malloc(i64 8)
 ret i8* %p
}
