define void @src() {
  store i8 0, ptr null
  ret void
}

define void @tgt() {
  call void @llvm.trap()
  unreachable
}

declare void @llvm.trap()
