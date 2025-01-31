define i64 @src(ptr captures(none) %p) {
  %v = ptrtoint ptr %p to i64
  ret i64 %v
}

define i64 @tgt(ptr captures(none) %p) {
  unreachable
}
