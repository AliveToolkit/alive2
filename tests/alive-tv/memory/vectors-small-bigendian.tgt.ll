target datalayout="E"

define i8 @f(ptr %p) {
  store i8 27, ptr %p
  ret i8 27
}

define i8 @f2(ptr %p) {
  store i8 27, ptr %p
  ret i8 27
}
