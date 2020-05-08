target datalayout = "e-p:64:64:64"

define i32 @src(i8* %p, i8* %q) {
	ret i32 0
}

define i32 @tgt(i8* %p, i8* %q) {
  %res = call i32 @memcmp(i8* %p, i8* %q, i64 0)
  ret i32 %res
}

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)
