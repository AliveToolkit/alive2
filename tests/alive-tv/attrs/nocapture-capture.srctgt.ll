define i64 @src(i8* nocapture %p) {
  %v = ptrtoint i8* %p to i64
  ret i64 %v
}

define i64 @tgt(i8* nocapture %p) {
  unreachable
}
