target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @free_noub() {
  %ptr = call noalias ptr @malloc(i64 4)
  %c = icmp eq ptr %ptr, null
  br i1 %c, label %RET, label %BB
RET:
  ret i8 0
BB:
  store i8 10, ptr %ptr
  %v = load i8, ptr %ptr
  call void @free(ptr %ptr)
  ret i8 %v
}

declare noalias ptr @malloc(i64)
declare void @free(ptr)

; ERROR: Value mismatch
