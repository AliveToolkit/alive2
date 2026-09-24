; TEST-ARGS: --disable-undef-input
; CHECK: Checking vscale = 1
; CHECK: Checking vscale = 2
; CHECK: Checking vscale = 4
; CHECK: Checking vscale = 8
; CHECK: Transformation seems to be correct!
; CHECK-NOT: Checking vscale = 16
; CHECK-NOT: ERROR:

define i1 @src() {
  %v = call i32 @llvm.vscale.i32()
  %prev = sub i32 %v, 1
  %bits = and i32 %v, %prev
  %power = icmp eq i32 %bits, 0
  %positive = icmp ugt i32 %v, 0
  %bounded = icmp ule i32 %v, 8
  %a = and i1 %power, %positive
  %r = and i1 %a, %bounded
  ret i1 %r
}
define i1 @tgt() {
  ret i1 true
}
