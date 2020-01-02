define i1 @f(i8* nocapture %p, i8* %q) {
  %c = icmp eq i8* %p, %q
  ret i1 %c
}
; ERROR: Value mismatch
