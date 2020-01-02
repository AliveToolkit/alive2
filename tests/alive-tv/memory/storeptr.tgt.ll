target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i64 @f(i64** %pptr) {
  %ptr0 = call i8* @malloc(i64 8)
  %c = icmp eq i8* %ptr0, null
  br i1 %c, label %BB1, label %BB2
BB1:
  ret i64 0
BB2:
  %ptr = bitcast i8* %ptr0 to i64*
  store i64 257, i64* %ptr, align 8
  store i64* %ptr, i64** %pptr, align 8
  %ptr2 = load i64*, i64** %pptr, align 8
  %i = load i64, i64* %ptr2, align 8
  ret i64 257
}

declare noalias i8* @malloc(i64)
