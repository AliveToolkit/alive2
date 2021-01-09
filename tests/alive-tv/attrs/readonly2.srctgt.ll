define i8 @src(i8* %p) readonly {
  store i8 0, i8* %p
  ret i8 2
}

define i8 @tgt(i8* %p) readonly {
  unreachable
}
