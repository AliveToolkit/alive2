declare ptr @llvm.thread.pointer()

define ptr @f() {
  %call = call ptr @llvm.thread.pointer()
  ret ptr %call
}
