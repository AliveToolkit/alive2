target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @calloc_init() {
  %ptr = call noalias ptr @calloc(i64 1, i64 1)
  %i = ptrtoint ptr %ptr to i64
  %cmp = icmp eq i64 %i, 0
  br i1 %cmp, label %BB1, label %BB2
BB1:
  ret i8 1
BB2:
  %v = load i8, ptr %ptr
  %w = add i8 %v, 2
  ret i8 %w
}

; ERROR: Value mismatch

declare noalias ptr @calloc(i64, i64)
declare void @free(ptr)
