target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @calloc_init(i8* %ptr0) {
  ret i8 1
}

declare noalias i8* @calloc(i64, i64)
declare void @free(i8*)
