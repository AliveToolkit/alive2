; TEST-ARGS: --max-vscale=4
; CHECK: 2 correct transformations
; CHECK: 1 incorrect transformations
; CHECK: 0 failed-to-prove transformations
; CHECK: 0 Alive2 errors
; ERROR: Value mismatch

define i32 @src1() {
  ret i32 0
}
define i32 @tgt1() {
  %v = call i32 @llvm.vscale.i32()
  %n = mul i32 %v, 2
  %last = sub i32 %n, 1
  %r = extractelement <vscale x 2 x i32> zeroinitializer, i32 %last
  ret i32 %r
}

define i32 @src2() {
  %r = extractelement <vscale x 2 x i32> zeroinitializer, i32 0
  ret i32 %r
}
define i32 @tgt2() {
  %v = call i32 @llvm.vscale.i32()
  %r = sub i32 %v, 1
  ret i32 %r
}

define i32 @src3() {
  ret i32 0
}
define i32 @tgt3() {
  ret i32 0
}
