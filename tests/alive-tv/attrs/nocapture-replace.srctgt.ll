define ptr @src(ptr captures(none) %p, ptr %q) {
  %c = icmp eq ptr %p, %q
  br i1 %c, label %A, label %B
A:
  ret ptr %q
B:
  ret ptr null
}
define ptr @tgt(ptr captures(none) %p, ptr %q) {
  %c = icmp eq ptr %p, %q
  br i1 %c, label %A, label %B
A:
  ret ptr %p
B:
  ret ptr null
}

; ERROR: Target is more poisonous than source
