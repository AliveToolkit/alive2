target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i64 @realloc-check() {
  %ptr = call noalias i8* @malloc(i64 4)
  %ptr2 = call noalias i8* @realloc(i8* %ptr, i64 0)
  %v = load i8, i8* %ptr
  ret i64 0
}

declare noalias i8* @malloc(i64)
declare noalias i8* @realloc(i8*, i64)
