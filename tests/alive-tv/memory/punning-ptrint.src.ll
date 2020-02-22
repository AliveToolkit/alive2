; TEST-ARGS: -smt-to=5000

define i8 @ptr_int_punning(i8** %pptr, i64 %n) {
  %ptr = alloca i8
  store i8* %ptr, i8** %pptr
  %pi8 = bitcast i8** %pptr to i8*
  %n2 = and i64 %n, 7
  %pi8_2 = getelementptr i8, i8* %pi8, i64 %n2
  %v = load i8, i8* %pi8_2
  ; %v is poison.
  ret i8 %v
}
