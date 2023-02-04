; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

define void @f(i8 %0, i8 %1, i8 %2, i8 %3, i8 %4, i8 %5, i8 %6, i8 %7, i8 %8, i64* %9) {
  %x = load i64, i64* %9, align 8
  %y = add nsw i64 %x, -777
  store i64 %y, i64* %9, align 8
  ret void
}
