target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i16 @f(i16** %pptr) {
  %ptr0 = call i8* @malloc(i64 2)
  %iaddr = ptrtoint i8* %ptr0 to i64
  %c = icmp eq i64 %iaddr, 0
  br i1 %c, label %BB1, label %BB2
BB1:
  ret i16 0
BB2:
  %ptr = bitcast i8* %ptr0 to i16*
  store i16 257, i16* %ptr, align 1
  store i16* %ptr, i16** %pptr, align 1
  %ptr2 = load i16*, i16** %pptr, align 1
  %i = load i16, i16* %ptr2, align 1
  ret i16 %i
}

declare noalias i8* @malloc(i64)
