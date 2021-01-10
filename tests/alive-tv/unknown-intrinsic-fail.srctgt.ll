; ERROR: Couldn't prove the correctness of the transformation

declare i8* @llvm.thread.pointer()

define i8* @src() {
  %call = call i8* @llvm.thread.pointer()
  ret i8* %call
}
define i8* @tgt() {
  ret i8* null
}
