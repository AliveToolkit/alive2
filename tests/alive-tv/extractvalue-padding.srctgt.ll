%struct = type { i32, [4 x i8] }

define i8 @src() {
  %A = extractvalue %struct { i32 2, [4 x i8] c"foo\00" }, 1, 2
  ret i8 %A
}

define i8 @tgt() {
  ret i8 111
}
