define void @src(i8* %ptr) {
  store i8 0, i8* %ptr
  call void @llvm.trap()
  unreachable
}

define void @tgt(i8* %ptr) {
  store i8 1, i8* %ptr
  call void @llvm.trap()
  unreachable
}

declare void @llvm.trap()

; ERROR: Source is more defined than target
