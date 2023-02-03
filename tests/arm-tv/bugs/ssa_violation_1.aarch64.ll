; TEST-ARGS:

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i32 @f() {
  br label %1

1:                                                ; preds = %1, %0
  br label %1
}
