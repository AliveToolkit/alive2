define i8 @src(ptr %p) {
  %v = load i8, ptr %p, !noundef !{}
  %r = freeze i8 %v
  ret i8 %r
}

define i8 @tgt(ptr %p) {
  %v = load i8, ptr %p, !noundef !{}
  ret i8 %v
}


