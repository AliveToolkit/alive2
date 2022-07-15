define void @src(ptr byval(i8) %a) {
  ret void
}

define void @tgt(ptr readnone byval(i8) %a) {
  ret void
}
