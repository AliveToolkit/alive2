target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i16 @f(ptr %pptr) {
  %ptr = call ptr @malloc(i64 2)
  %c = icmp eq ptr %ptr, null
  br i1 %c, label %BB1, label %BB2
BB1:
  ret i16 0
BB2:
  store i16 257, ptr %ptr, align 1
  store ptr %ptr, ptr %pptr, align 1
  %ptr2 = load ptr, ptr %pptr, align 1
  %i = load i16, ptr %ptr2, align 1
  ret i16 258
}

declare noalias ptr @malloc(i64)
