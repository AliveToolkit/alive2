; TEST-ARGS: -disable-undef-input -disable-poison-input -smt-to=10000

target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @ptr_int_punning(i8** %pptr, i64 %n) {
  %ptr = call i8* @malloc(i64 1)
  %cmp = icmp eq i8* %ptr, null
  br i1 %cmp, label %BB1, label %BB2
BB1:
  ret i8 0
BB2:
  store i8* %ptr, i8** %pptr
  %pi8 = bitcast i8** %pptr to i8*
  %n2 = and i64 %n, 7
  %pi8_2 = getelementptr i8, i8* %pi8, i64 %n2
  %v = load i8, i8* %pi8_2
  ; %v is poison.
  ret i8 %v
}
declare noalias i8* @malloc(i64)
