target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @src(i64 %num) {
  %ptr = call noalias ptr @calloc(i64 555555555555555, i64 40000000)
  %unused = ptrtoint ptr %ptr to i64
  %cmp = icmp eq ptr %ptr, null
  br i1 %cmp, label %BB1, label %BB2
BB1:
  ret i8 0
BB2:
  ret i8 1
}

define i8 @tgt(i64 %num) {
  ret i8 1
}

; ERROR: Value mismatch

declare noalias ptr @calloc(i64, i64)
