target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i64 @realloc-check() {
  ret i64 -1
}

declare noalias i8* @malloc(i64)
declare noalias i8* @realloc(i8*, i64)
