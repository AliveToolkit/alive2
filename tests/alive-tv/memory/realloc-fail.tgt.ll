target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @realloc-fail() {
  %ptr = call noalias ptr @malloc(i64 4)
  %cmp = icmp eq ptr %ptr, null
  br i1 %cmp, label %BB1, label %BB2
BB1:
  ret i8 -1
BB2:
  store i8 5, ptr %ptr
  %ptr2 = call noalias ptr @realloc(ptr %ptr, i64 100)
  ret i8 6
}

declare noalias ptr @malloc(i64)
declare noalias ptr @realloc(ptr, i64)
