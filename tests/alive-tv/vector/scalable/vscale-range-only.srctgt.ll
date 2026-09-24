; TEST-ARGS: --max-vscale=4
; CHECK: Transformation seems to be correct!
; CHECK-NOT: Checking vscale
; CHECK-NOT: ERROR:

; vscale_range alone does not make a function depend on vscale, so the pair
; is checked once rather than at every scale.
define i32 @src(i32 %x) vscale_range(1, 16) {
  %r = add i32 %x, 0
  ret i32 %r
}
define i32 @tgt(i32 %x) vscale_range(1, 16) {
  ret i32 %x
}
