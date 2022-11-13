; ERROR: Couldn't prove the correctness of the transformation

declare ptr @llvm.objc.loadWeak(ptr)

define ptr @src() {
  %call = call ptr @llvm.objc.loadWeak(ptr null)
  ret ptr %call
}
define ptr @tgt() {
  ret ptr null
}
