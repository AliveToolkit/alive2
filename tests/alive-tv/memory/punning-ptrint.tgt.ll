define i8 @ptr_int_punning(i8** %pptr, i64 %n) {
  %ptr = alloca i8
  store i8* %ptr, i8** %pptr
  ret i8 undef
}
declare noalias i8* @malloc(i64)
