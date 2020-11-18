target datalayout = "e-p:64:64:64"
target triple = "x86_64-unknown-linux-gnu"

define i1 @src(i64 %x, i64 %y, i64 %n) {
  %p = alloca i64
  %q = alloca i64
  store i64 %x, i64* %p
  store i64 %y, i64* %q
  %p8 = bitcast i64* %p to i8*
  %q8 = bitcast i64* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 %n)
  %c = icmp eq i32 %res, 0
  ret i1 %c
}

define i1 @tgt(i64 %x, i64 %y, i64 %n) {
  %p = alloca i64
  %q = alloca i64
  store i64 %x, i64* %p
  store i64 %y, i64* %q
  %p8 = bitcast i64* %p to i8*
  %q8 = bitcast i64* %q to i8*
  %res = call i32 @bcmp(i8* %p8, i8* %q8, i64 %n)
  %c = icmp eq i32 %res, 0
  ret i1 %c
}

declare i32 @bcmp(i8* nocapture, i8* nocapture, i64)
declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)
