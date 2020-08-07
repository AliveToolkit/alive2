declare i8 @llvm.ushl.sat.i8(i8, i8)

define i8 @src(i8 %val, i8 %shamt) {
  %t0 = shl i8 %val, %shamt
  %t1 = lshr i8 %t0, %shamt
  %cmp = icmp eq i8 %val, %t1
  %r = select i1 %cmp, i8 %t0, i8 -1
  ret i8 %r
}

define i8 @tgt(i8 %val, i8 %shamt) {
  %r = call i8 @llvm.ushl.sat.i8(i8 %val, i8 %shamt)
  ret i8 %r
}
