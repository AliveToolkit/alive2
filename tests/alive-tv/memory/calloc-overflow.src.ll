; TEST-ARGS: -smt-to=9000
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @calloc_overflow(i64 %num) {
  %ptr = call noalias i8* @calloc(i64 555555555555555, i64 40000000)
  %i = ptrtoint i8* %ptr to i64
  %cmp = icmp eq i64 %i, 0
  br i1 %cmp, label %BB1, label %BB2
BB1:
  ret i8 0
BB2:
  ret i8 1
}

; ERROR: Value mismatch

declare noalias i8* @calloc(i64, i64)
declare void @free(i8*)
