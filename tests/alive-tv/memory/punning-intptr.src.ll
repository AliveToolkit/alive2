; TEST-ARGS: -smt-to=9000 -disable-undef-input

target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8* @int_ptr_punning(i8* %ptr, i64 %n) {
  store i8 1, i8* %ptr
  %n2 = and i64 %n, 7
  %n3 = sub i64 0, %n2
  %ptr_2 = getelementptr i8, i8* %ptr, i64 %n3
  %pptr = bitcast i8* %ptr_2 to i8**
  %v = load i8*, i8** %pptr
  ; %v is poison.
  ret i8* %v
}
