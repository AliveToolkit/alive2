define noundef {i8, i32} @src() {
  %a = insertvalue {i8, i32} undef, i8  10, 0
  %b = insertvalue {i8, i32} %a, i32 undef, 1
  ret {i8, i32} %b
}

define noundef {i8, i32} @tgt() {
  unreachable
}
