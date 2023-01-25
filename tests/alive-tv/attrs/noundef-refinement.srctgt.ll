define i1 @src(i8 noundef %0) {
  %_t1 = trunc i8 %0 to i1
  ret i1 %_t1
}

define i1 @tgt(i8 %0) {
  %_t1 = trunc i8 %0 to i1
  ret i1 %_t1
}

