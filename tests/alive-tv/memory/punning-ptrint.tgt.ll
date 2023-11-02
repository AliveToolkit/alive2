define i8 @ptr_int_punning(ptr %pptr, i64 %n) {
  %ptr = alloca i8
  store ptr %ptr, ptr %pptr
  ret i8 undef
}
declare noalias ptr @malloc(i64)
