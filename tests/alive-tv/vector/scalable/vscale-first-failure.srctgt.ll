; CHECK: Checking vscale = 1
; CHECK: Checking vscale = 4
; CHECK: Checking vscale = 8
; CHECK-NOT: Checking vscale = 16
; ERROR: Value mismatch

define i1 @src() {
  ret i1 true
}
define i1 @tgt() {
  %v = call i32 @llvm.vscale.i32()
  %r = icmp ult i32 %v, 8
  ret i1 %r
}
