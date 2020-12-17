define void @src(i8* %ptr) {
  call void @llvm.trap()
  unreachable
}

define void @tgt(i8* %ptr) {
  unreachable
}


declare void @llvm.trap()

; ERROR: Source is more defined than target
