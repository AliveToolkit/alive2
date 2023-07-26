define i64 @src(ptr nocapture %p) {
  %v = ptrtoint ptr %p to i64
  ret i64 %v
}

define i64 @tgt(ptr nocapture %p) {
  unreachable
}
