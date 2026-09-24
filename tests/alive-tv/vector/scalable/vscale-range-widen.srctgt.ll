; TEST-ARGS: --max-vscale=4
; CHECK: Checking vscale = 2
; CHECK: Transformation seems to be correct!
; CHECK-NOT: Checking vscale = 1
; CHECK-NOT: Checking vscale = 4
; CHECK-NOT: ERROR:

define i32 @src() vscale_range(2) {
  %v = call i32 @llvm.vscale.i32()
  ret i32 %v
}
define i32 @tgt() vscale_range(1, 0) {
  ret i32 2
}
