declare i8 @llvm.ssub.sat.i8(i8, i8)
define i1 @src_sge(i8 %x, i8 %y) {
  %m = call i8 @llvm.ssub.sat.i8(i8 %x, i8 %y)
  %r = icmp sge i8 %m, 0
  ret i1 %r
}

define i1 @tgt_sge(i8 %x, i8 %y) {
  %r = icmp sge i8 %x, %y
  ret i1 %r
}

define i1 @src_sgt(i8 %x, i8 %y) {
  %m = call i8 @llvm.ssub.sat.i8(i8 %x, i8 %y)
  %r = icmp sgt i8 %m, 0
  ret i1 %r
}

define i1 @tgt_sgt(i8 %x, i8 %y) {
  %r = icmp sgt i8 %x, %y
  ret i1 %r
}

; unused
define i1 @src(i8 %x, i8 %y) {
  %m = call i8 @llvm.ssub.sat.i8(i8 %x, i8 %y)
  %r = icmp sgt i8 %m, 0
  ret i1 %r
}
