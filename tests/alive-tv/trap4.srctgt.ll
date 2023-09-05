define void @src(ptr %ptr) {
  call void @llvm.trap()
  unreachable
}

define void @tgt(ptr %ptr) {
  unreachable
}


declare void @llvm.trap()

; ERROR: Source is more defined than target
