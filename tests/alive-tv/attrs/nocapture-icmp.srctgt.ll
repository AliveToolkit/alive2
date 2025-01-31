define i1 @src(ptr captures(none) %p, ptr %q) {
  %c = icmp eq ptr %p, %q
  ret i1 %c
}
define i1 @tgt(ptr captures(none) %p, ptr %q) {
  ret i1 false
}
; ERROR: Value mismatch
