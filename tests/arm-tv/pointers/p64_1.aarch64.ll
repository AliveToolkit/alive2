; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

define void @f(i64* %0) {
  %2 = load i64, i64* %0, align 8
  %3 = add nsw i64 %2, -777
  store i64 %3, i64* %0, align 8
  ret void
}
