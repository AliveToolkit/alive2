define void @src(i8* %dummyptr) {
  call void @llvm.trap()
  store i8 0, i8* %dummyptr
  ret void
}

define void @tgt(i8* %dummyptr) {
  call void @llvm.trap()
  unreachable
}

declare void @llvm.trap()
