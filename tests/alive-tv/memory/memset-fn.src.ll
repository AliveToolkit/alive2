target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

define ptr @f(ptr %ptr, i32 %val, i64 %len) {
  %call = call ptr @memset(ptr %ptr, i32 %val, i64 %len)
  ret ptr %call
}

declare ptr @memset(ptr, i32, i64)
