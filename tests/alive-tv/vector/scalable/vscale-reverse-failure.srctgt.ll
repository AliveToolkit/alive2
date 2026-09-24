; TEST-ARGS: --max-vscale=4 --bidirectional
; CHECK: Checking vscale = 2
; CHECK: Reverse transformation doesn't verify!
; ERROR: Target is more poisonous than source

define i1 @src() {
  %v = call i1 @llvm.vscale.i1()
  ret i1 %v
}
define i1 @tgt() {
  ret i1 true
}
