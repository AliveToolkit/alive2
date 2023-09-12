define void @src(ptr %ptr) {
  store i8 0, ptr %ptr
  call void @llvm.trap()
  unreachable
}

define void @tgt(ptr %ptr) {
  store i8 1, ptr %ptr
  call void @llvm.trap()
  unreachable
}

declare void @llvm.trap()
