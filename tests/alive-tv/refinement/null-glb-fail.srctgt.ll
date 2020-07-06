@glb = global i8 0

define i8* @src(i8* %p) {
entry:
  %c = icmp eq i8* %p, null
  br i1 %c, label %if.then, label %if.end
if.then:
  ret i8* %p
if.end:
  ret i8* @glb
}

define i8* @tgt(i8* %p) {
  ret i8* @glb
}

; ERROR: Value mismatch
