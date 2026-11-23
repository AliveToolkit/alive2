; TEST-ARGS: --max-vscale=4
; SKIP-IDENTITY
; CHECK-NOT: ERROR:
; CHECK-NOT: Checking vscale =
; CHECK: Transformation seems to be correct!

; A vscale_range minimum above --max-vscale is fine when the function
; doesn't depend on vscale.
define i32 @src(i32 %x) vscale_range(8,8) {
  %r = add i32 %x, 0
  ret i32 %r
}
define i32 @tgt(i32 %x) vscale_range(8,8) {
  ret i32 %x
}
