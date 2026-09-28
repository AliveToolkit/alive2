; TEST-ARGS: --single-vscale=4
; CHECK: Transformation seems to be correct!

declare i64 @llvm.vscale.i64()

define i64 @src() {
  %v = call i64 @llvm.vscale.i64()
  ret i64 %v
}

define i64 @tgt() {
  ret i64 4
}
