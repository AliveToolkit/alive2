; TEST-ARGS: --single-vscale=2
; ERROR: Target is more poisonous than source

declare i1 @llvm.vscale.i1()

define i1 @src() {
  ret i1 true
}

define i1 @tgt() {
  %v = call i1 @llvm.vscale.i1()
  ret i1 %v
}
