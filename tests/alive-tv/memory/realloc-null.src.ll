target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @realloc-null() {
  %ptr = call noalias i8* @realloc(i8* null, i64 3)
  store i8 5, i8* %ptr
  %v = load i8, i8* %ptr
  ret i8 %v
}

declare noalias i8* @realloc(i8*, i64)
