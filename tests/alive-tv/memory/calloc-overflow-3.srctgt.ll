target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @src(i8*) {
  %ptr = call noalias i8* @calloc(i64 555555555555555, i64 40000000)
  call void @free(i8* null)
  ret i8 1
}

define i8 @tgt(i8*) {
  ret i8 1
}

declare void @free(i8*)
declare noalias i8* @calloc(i64, i64)
