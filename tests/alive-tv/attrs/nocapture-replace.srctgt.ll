define i8* @src(i8* nocapture %p, i8* %q) {
  %c = icmp eq i8* %p, %q
  br i1 %c, label %A, label %B
A:
  ret i8* %q
B:
  ret i8* null
}
define i8* @tgt(i8* nocapture %p, i8* %q) {
  %c = icmp eq i8* %p, %q
  br i1 %c, label %A, label %B
A:
  ret i8* %p
B:
  ret i8* null
}

; ERROR: Source is more defined than target
