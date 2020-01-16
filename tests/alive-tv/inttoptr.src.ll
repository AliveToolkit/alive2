define i64* @f(i64 %x, i64 %y) {
  %p = inttoptr i64 %x to i64*
  ret i64* %p
}

; XFAIL: Invalid expr
