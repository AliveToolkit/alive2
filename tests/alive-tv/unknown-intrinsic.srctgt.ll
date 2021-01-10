declare i8* @llvm.thread.pointer()

define i8* @f() {
  %call = call i8* @llvm.thread.pointer()
  ret i8* %call
}
