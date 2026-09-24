; TEST-ARGS: --max-vscale=4
; CHECK: Checking vscale = 4
; CHECK: %v = i32 4
; CHECK: Transformation seems to be correct!
; CHECK-NOT: ERROR:

define i32 @src() {
  ret i32 42
}
define i32 @tgt() {
  %v = call i32 @llvm.vscale.i32()
  %n = mul i32 %v, 2
  %last = sub i32 %n, 1
  %vec = insertelement <vscale x 2 x i32> poison, i32 42, i32 %last
  %r = extractelement <vscale x 2 x i32> %vec, i32 %last
  ret i32 %r
}
