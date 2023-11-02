target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i64 @realloc-check() {
  %ptr = call noalias ptr @malloc(i64 4)
  %ptr2 = call noalias ptr @realloc(ptr %ptr, i64 0)
  %v = load i8, ptr %ptr
  ret i64 0
}

declare noalias ptr @malloc(i64)
declare noalias ptr @realloc(ptr, i64)
