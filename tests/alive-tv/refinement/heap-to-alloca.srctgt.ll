declare i8* @malloc(i64)

define i8* @src(i64 %x) {
 %p = call i8* @malloc(i64 8)
 %c = icmp ne i8* %p, null
 br i1 %c, label %RET, label %UNREACHABLE
RET:
 ret i8* %p
UNREACHABLE:
 unreachable
}

define i8* @tgt(i64 %x) {
 %p = alloca i64
 %p8 = bitcast i64* %p to i8*
 ret i8* %p8
}

; ERROR: Value mismatch
