define i32 @src(i32 %t) {
  switch i32 %t, label %DEFAULT [
    i32 1, label %ONE
    i32 2, label %TWO
  ]
DEFAULT:
  %v = freeze i32 %t
  ret i32 %v
ONE:
  ret i32 1
TWO:
  ret i32 2
}

define i32 @tgt(i32 %t) {
  switch i32 %t, label %DEFAULT [
    i32 1, label %ONE
    i32 2, label %TWO
  ]
DEFAULT:
  ret i32 %t
ONE:
  ret i32 1
TWO:
  ret i32 2
}
