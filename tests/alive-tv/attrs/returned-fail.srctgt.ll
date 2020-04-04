declare i8 @f(i8 returned)

define i8 @src(i8 %t) {
  %v = call i8 @f(i8 %t)
  ret i8 %v
}

define i8 @tgt(i8 %t) {
  call i8 @f(i8 %t)
  ret i8 0
}

; ERROR: Value mismatch
