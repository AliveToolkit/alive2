define i64 @f(i64 %x) {
  %p = inttoptr i64 %x to i64*
  %a = load i64, i64* %p
  ret i64 %a
}

; XFAIL: Invalid expr
