; TEST-ARGS: --single-vscale=2 --max-vscale=4
; SKIP-IDENTITY
; CHECK: Alive2: single-vscale and max-vscale cannot both be specified!

define i32 @src() {
  ret i32 0
}
define i32 @tgt() {
  ret i32 0
}
