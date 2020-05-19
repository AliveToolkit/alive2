target datalayout = "e-p:64:64:64"

define i32 @ub_null() {
  %p = alloca i32
	store i32 0, i32* %p
	%p8 = bitcast i32* %p to i8*
  %res = call i32 @memcmp(i8* %p8, i8* null, i64 4) ; ub
	ret i32 %res
}

define i32 @ub_oob() {
  %p = alloca i32
	store i32 0, i32* %p
  %p8 = bitcast i32* %p to i8*
  %q = getelementptr i8, i8* %p8, i64 4
  %res = call i32 @memcmp(i8* %p8, i8* %q, i64 4) ; ub
	ret i32 %res
}

define i32 @ub_oob2() {
  %p = alloca i32
	store i32 0, i32* %p
  %p8 = bitcast i32* %p to i8*
  %q = getelementptr i8, i8* %p8, i64 1
  %res = call i32 @memcmp(i8* %p8, i8* %q, i64 4) ; ub!
	ret i32 %res
}

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)
