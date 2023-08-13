define i1 @src(ptr nocapture %p, ptr %q) {
  %c = icmp eq ptr %p, %q
  ret i1 %c
}
define i1 @tgt(ptr nocapture %p, ptr %q) {
  ret i1 false
}
; ERROR: Value mismatch
