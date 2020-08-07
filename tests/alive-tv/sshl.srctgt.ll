declare i8 @llvm.sshl.sat.i8(i8, i8)

define i8 @src(i8 %val, i8 %shamt) {
  %t0 = shl i8 %val, %shamt
  %t1 = ashr i8 %t0, %shamt
  %cmp = icmp eq i8 %val, %t1
  %cmp1 = icmp slt i8 %val, 0
  %t2 = select i1 %cmp1, i8 -128, i8 127
  %r = select i1 %cmp, i8 %t0, i8 %t2
  ret i8 %r
}

define i8 @tgt(i8 %val, i8 %shamt) {
  %r = call i8 @llvm.sshl.sat.i8(i8 %val, i8 %shamt)
  ret i8 %r
}
