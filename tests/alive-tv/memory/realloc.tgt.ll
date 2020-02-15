target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @realloc-check() {
  %ptr = call noalias i8* @malloc(i64 4)
  %cmp = icmp eq i8* %ptr, null
  br i1 %cmp, label %BB1, label %BB2
BB1:
  ret i8 -1
BB2:
  store i8 5, i8* %ptr
  %ptr2 = call noalias i8* @realloc(i8* %ptr, i64 1)
  %cmp2 = icmp eq i8* %ptr2, null
  br i1 %cmp2, label %BB3, label %BB4
BB3:
  ret i8 6
BB4:
  ret i8 7
}

declare noalias i8* @malloc(i64)
declare noalias i8* @realloc(i8*, i64)
