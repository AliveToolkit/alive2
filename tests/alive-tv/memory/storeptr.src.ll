target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i64 @f(ptr %pptr) {
  %ptr = call ptr @malloc(i64 8)
  %c = icmp eq ptr %ptr, null
  br i1 %c, label %BB1, label %BB2
BB1:
  ret i64 0
BB2:
  store i64 257, ptr %ptr, align 8
  store ptr %ptr, ptr %pptr, align 8
  %ptr2 = load ptr, ptr %pptr, align 8
  %i = load i64, ptr %ptr2, align 8
  ret i64 %i
}

declare noalias ptr @malloc(i64)
