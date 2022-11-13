; ERROR: Source is more defined than target

define i8 @src(ptr %p) memory(read) {
  %a = alloca i8
  store i8 0, ptr %a
  ret i8 2
}

define i8 @tgt(ptr %p) memory(read) {
  %a = alloca i8
  unreachable
}
