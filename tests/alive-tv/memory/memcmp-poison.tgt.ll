target datalayout = "e-p:64:64:64"

define i32 @poison_1() {
  ret i32 1
}

define i32 @poison_0() {
  ret i32 0
}

define i32 @poison_m1() {
  ret i32 -1
}

define i32 @poison_p() {
  %p = alloca i32
  %p8 = bitcast i32* %p to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %p8, i64 4) ; poison
  ret i32 %res
}


define i32 @poison_partial_1() {
  ret i32 1
}

define i32 @poison_partial_0() {
  ret i32 0
}

define i32 @poison_partial_m1() {
  ret i32 -1
}

define i32 @poison_partial_p() {
  %p = alloca i32
  %p8 = bitcast i32* %p to i8*
  store i8 0, i8* %p8
  %res = call i32 @memcmp(i8* %p8, i8* %p8, i64 4) ; poison
  ret i32 %res
}

define i32 @poison_diffblocks() {
  %p = alloca i32
  %q = alloca i32
  %p16 = bitcast i32* %p to i16*
  %q16 = bitcast i32* %q to i16*
  store i16 257, i16* %p16 ; 01 01 pp pp
  store i16 257, i16* %q16 ; 01 01 pp pp
  %p8 = bitcast i32* %p to i8*
  %q8 = bitcast i32* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 4)
  ret i32 %res
}

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)

