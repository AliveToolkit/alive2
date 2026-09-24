; TEST-ARGS: --max-vscale=4 --bidirectional
; CHECK-NOT: Transformation seems to be correct!
; ERROR: Source vscale_range excludes vscale = 1

define i32 @src() vscale_range(2) {
  %v = call i32 @llvm.vscale.i32()
  ret i32 %v
}
define i32 @tgt() vscale_range(1, 2) {
  %v = call i32 @llvm.vscale.i32()
  ret i32 %v
}
