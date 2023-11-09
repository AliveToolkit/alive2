declare void @llvm.assert(i1)

; LLVM isn't free to remove @llvm.assert, but it's still a refinement
; from Alive's point of view

define void @src(i16, i16) {
  %c = icmp eq i16 %0, %1
  call void @llvm.assert(i1 %c)
  ret void
}

define void @tgt(i16, i16) {
  ret void
}


