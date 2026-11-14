define i32 @src({ i8, i16, i8, i32 } %x) {
  %a = insertvalue { i8, i16, i8, i32 } %x, i32 7, 3
  %b = extractvalue { i8, i16, i8, i32 } %a, 3
  ret i32 %b
}

define i32 @tgt({ i8, i16, i8, i32 } %x) {
  ret i32 7
}
