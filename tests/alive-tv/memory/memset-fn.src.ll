; TEST-ARGS: -smt-to=5000
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

define i32* @f(i32* %ptr, i32 %val, i64 %len) {
  %p = bitcast i32* %ptr to i8*
  %call = call i8* @memset(i8* %p, i32 %val, i64 %len)
  %cast = bitcast i8* %call to i32*
  ret i32* %cast
}

declare i8* @memset(i8*, i32, i64)
