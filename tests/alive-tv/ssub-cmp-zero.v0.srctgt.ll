declare i8 @llvm.ssub.sat.i8(i8, i8)
define i1 @src(i8 %x, i8 %y) {
  %m = call i8 @llvm.ssub.sat.i8(i8 %x, i8 %y)
  %r = icmp eq i8 %m, 0
  ret i1 %r
}

define i1 @tgt(i8 %x, i8 %y) {
  %r = icmp eq i8 %x, %y
  ret i1 %r
}

define i1 @src1(i8 %x, i8 %y) {
  %m = call i8 @llvm.ssub.sat.i8(i8 %x, i8 %y)
  %r = icmp ne i8 %m, 0
  ret i1 %r
}

define i1 @tgt1(i8 %x, i8 %y) {
  %r = icmp ne i8 %x, %y
  ret i1 %r
}

define i1 @src2(i8 %x, i8 %y) {
  %m = call i8 @llvm.ssub.sat.i8(i8 %x, i8 %y)
  %r = icmp sle i8 %m, 0
  ret i1 %r
}

define i1 @tgt2(i8 %x, i8 %y) {
  %r = icmp sle i8 %x, %y
  ret i1 %r
}

define i1 @src3(i8 %x, i8 %y) {
  %m = call i8 @llvm.ssub.sat.i8(i8 %x, i8 %y)
  %r = icmp slt i8 %m, 0
  ret i1 %r
}

define i1 @tgt3(i8 %x, i8 %y) {
  %r = icmp slt i8 %x, %y
  ret i1 %r
}
