; TEST-ARGS: --bidirectional
; SKIP-IDENTITY
; CHECK-NOT: ERROR:
; CHECK: 3 correct transformations
; CHECK: 0 incorrect transformations
; CHECK: 0 failed-to-prove transformations
; CHECK: 0 Alive2 errors

; Functions that don't depend on vscale verify identically at every scale,
; so vscale_range is not checked against the default single-vscale (2), and
; changes to it are allowed.
define i32 @src_outside(i32 %x) vscale_range(4,4) {
  ret i32 %x
}
define i32 @tgt_outside(i32 %x) vscale_range(4,4) {
  ret i32 %x
}

define i32 @src_change(i32 %x) vscale_range(1,4) {
  ret i32 %x
}
define i32 @tgt_change(i32 %x) vscale_range(2,4) {
  ret i32 %x
}

define <4 x i32> @src_fixed(<4 x i32> %x) {
  ret <4 x i32> %x
}
define <4 x i32> @tgt_fixed(<4 x i32> %x) vscale_range(8,0) {
  ret <4 x i32> %x
}
