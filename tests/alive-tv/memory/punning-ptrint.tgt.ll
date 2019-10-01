target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @ptr_int_punning(i8** %pptr, i64 %n) {
  %ptr = call i8* @malloc(i64 1)
  %i = ptrtoint i8* %ptr to i64
  %cmp = icmp eq i64 %i, 0
  br i1 %cmp, label %BB1, label %BB2
BB1:
  ret i8 0
BB2:
  store i8* %ptr, i8** %pptr
  ret i8 undef
}
declare noalias i8* @malloc(i64)
