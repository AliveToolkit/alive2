@glb = global i8 0

define ptr @src(ptr %p) {
entry:
  %c = icmp eq ptr %p, null
  br i1 %c, label %if.then, label %if.end
if.then:
  ret ptr %p
if.end:
  ret ptr @glb
}

define ptr @tgt(ptr %p) {
  ret ptr @glb
}

; ERROR: Value mismatch
