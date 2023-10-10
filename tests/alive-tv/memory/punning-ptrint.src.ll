define i8 @ptr_int_punning(ptr %pptr, i64 %n) {
  %ptr = alloca i8
  store ptr %ptr, ptr %pptr
  %n2 = and i64 %n, 7
  %pi8_2 = getelementptr i8, ptr %pptr, i64 %n2
  %v = load i8, ptr %pi8_2
  ; %v is poison.
  ret i8 %v
}
