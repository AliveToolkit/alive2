; target: 64 bits ptr addr
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @freshbid_malloc(ptr %ptr0) {
  %ptr = call noalias ptr @malloc(i64 1)
  %cmp = icmp eq ptr %ptr, null
  br i1 %cmp, label %BB1, label %BB2
BB1:
  ret i8 0
BB2:
  store i8 10, ptr %ptr
  store i8 20, ptr %ptr0
  %v = load i8, ptr %ptr
  ret i8 10
}

declare noalias ptr @malloc(i64)
declare void @free(ptr)
