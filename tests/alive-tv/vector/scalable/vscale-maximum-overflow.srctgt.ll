; TEST-ARGS: --max-vscale=4294967295
; CHECK: Checking vscale = 2147483648
; CHECK: Transformation seems to be correct!
; CHECK-NOT: ERROR:

define i64 @src() vscale_range(2147483648) {
  %v = call i64 @llvm.vscale.i64()
  ret i64 %v
}
define i64 @tgt() vscale_range(2147483648) {
  ret i64 2147483648
}
