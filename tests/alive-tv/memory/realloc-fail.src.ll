target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @realloc-fail() {
  %ptr = call noalias i8* @malloc(i64 4)
  %cmp = icmp eq i8* %ptr, null
  br i1 %cmp, label %BB1, label %BB2
BB1:
  ret i8 -1
BB2:
  store i8 5, i8* %ptr
  %ptr2 = call noalias i8* @realloc(i8* %ptr, i64 100)
  %cmp2 = icmp eq i8* %ptr2, null
  br i1 %cmp2, label %BB3, label %BB4
BB3:
  %a = load i8, i8* %ptr
  store i8 1, i8* %ptr
  %b = load i8, i8* %ptr
  %s = add i8 %a, %b
  ret i8 %s
BB4:
  %a2 = load i8, i8* %ptr2
  store i8 2, i8* %ptr2
  %b2 = load i8, i8* %ptr2
  %s2 = add i8 %a2, %b2
  ret i8 %s2
}

declare noalias i8* @malloc(i64)
declare noalias i8* @realloc(i8*, i64)

; ERROR: Value mismatch
