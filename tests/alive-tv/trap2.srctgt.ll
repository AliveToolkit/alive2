define void @src() {
  store i8 0, i8* null
  ret void
}

define void @tgt() {
  call void @llvm.trap()
  unreachable
}

declare void @llvm.trap()
