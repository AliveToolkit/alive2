define ptr @src(ptr %a, ptr captures(none) %b) {
  %cmp = icmp eq ptr %a, %b
  br i1 %cmp, label %t, label %f

t:
  %v = call ptr @g(ptr %b)
  ret ptr %v

f:
  ret ptr null
}

define ptr @tgt(ptr %a, ptr captures(none) %b) {
  %cmp = icmp eq ptr %a, %b
  br i1 %cmp, label %t, label %f

t:
  %v = call ptr @g(ptr %a)
  ret ptr %v

f:
  ret ptr null
}


declare ptr @g(ptr captures(none))

; If %a = %b + n, this is wrong.
; ERROR: Source is more defined than target
