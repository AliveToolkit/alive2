target datalayout = "e-p:64:64:64"

define i32 @ub() {
  %p = alloca i32
  %p8 = bitcast i32* %p to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %p8, i64 4) ; ub
  ret i32 %res
}

define i32 @ub2() {
  %p = alloca i32
  %p8 = bitcast i32* %p to i8*
  store i8 0, i8* %p8
  %res = call i32 @memcmp(i8* %p8, i8* %p8, i64 4) ; ub
  ret i32 %res
}

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

define i32 @ub_diffblocks() {
  %p = alloca i32
  %q = alloca i32
  %p16 = bitcast i32* %p to i16*
  %q16 = bitcast i32* %q to i16*
  store i16 257, i16* %p16 ; 01 01 pp pp
  store i16 257, i16* %q16 ; 01 01 pp pp
  %p8 = bitcast i32* %p to i8*
  %q8 = bitcast i32* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 4)
  %res2 = add i32 %res, %res
  ret i32 %res2
}

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)
