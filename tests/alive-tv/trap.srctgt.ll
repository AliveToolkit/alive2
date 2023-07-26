define void @src(ptr %dummyptr) {
  call void @llvm.trap()
  store i8 0, ptr %dummyptr
  ret void
}

define void @tgt(ptr %dummyptr) {
  call void @llvm.trap()
  unreachable
}

declare void @llvm.trap()
