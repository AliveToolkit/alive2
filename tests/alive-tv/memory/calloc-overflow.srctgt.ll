target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @src(i64 %num) {
  %ptr = call noalias i8* @calloc(i64 555555555555555, i64 40000000)
  %cmp = icmp eq i8* %ptr, null
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

declare noalias i8* @calloc(i64, i64)
