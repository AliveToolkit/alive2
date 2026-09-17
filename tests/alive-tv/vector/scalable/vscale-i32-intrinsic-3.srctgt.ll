declare i32 @llvm.vscale.i32()

define i32 @src() {
  %v = call i32 @llvm.vscale.i32()
  ret i32 %v
}

define i32 @tgt() {
  ret i32 3
}


; TEST-ARGS: --vscale=3
