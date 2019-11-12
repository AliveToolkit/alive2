target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @free_noub() {
  %ptr = call noalias i8* @calloc(i64 4, i64 1)
  call void @free(i8* %ptr)
  ret i8 0
}

; ERROR: Value mismatch


declare noalias i8* @calloc(i64, i64)
declare void @free(i8*)
