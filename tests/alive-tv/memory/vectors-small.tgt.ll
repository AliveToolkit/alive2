target datalayout="e"

define i8 @f(ptr %p) {
  store i8 57, ptr %p
  ret i8 57
}

define i8 @f2(ptr %p) {
  store i8 57, ptr %p
  ret i8 57
}
