; TEST-ARGS: --quiet --disable-undef-input --single-vscale=1024
; CHECK: Transformation seems to be correct!
; CHECK-NOT: ERROR:

; 32 minimum lanes at vscale 1024 is 32768 realized lanes, inside the limit on
; an aggregate's element count.
define i1 @src() {
  ret i1 false
}

define i1 @tgt() {
  %r = extractelement <vscale x 32 x i1> zeroinitializer, i32 32767
  ret i1 %r
}
