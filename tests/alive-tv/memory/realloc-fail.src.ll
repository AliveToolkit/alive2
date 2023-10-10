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
  %cmp2 = icmp eq ptr %ptr2, null
  br i1 %cmp2, label %BB3, label %BB4
BB3:
  %a = load i8, ptr %ptr
  store i8 1, ptr %ptr
  %b = load i8, ptr %ptr
  %s = add i8 %a, %b
  ret i8 %s
BB4:
  %a2 = load i8, ptr %ptr2
  store i8 2, ptr %ptr2
  %b2 = load i8, ptr %ptr2
  %s2 = add i8 %a2, %b2
  ret i8 %s2
}

declare noalias ptr @malloc(i64)
declare noalias ptr @realloc(ptr, i64)

; ERROR: Value mismatch
