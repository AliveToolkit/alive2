define i8* @src(i8* %in) {
  %c = icmp eq i8* %in, null
  %r = select i1 %c, i8* null, i8* %in
  ret i8* %r
}

define i8* @tgt(i8* %in) {
  ret i8* %in
}
