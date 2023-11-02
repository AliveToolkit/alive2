target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @realloc-null() {
  %ptr = call noalias ptr @malloc(i64 3)
  store i8 5, ptr %ptr
  %v = load i8, ptr %ptr
  ret i8 %v
}

declare noalias ptr @malloc(i64)
