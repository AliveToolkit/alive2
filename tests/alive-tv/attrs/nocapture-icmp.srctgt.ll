define i1 @src(i8* nocapture %p, i8* %q) {
  %c = icmp eq i8* %p, %q
  ret i1 %c
}
define i1 @tgt(i8* nocapture %p, i8* %q) {
  ret i1 false
}
; ERROR: Value mismatch
