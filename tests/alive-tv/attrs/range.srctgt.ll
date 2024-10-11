; ERROR: Target is more poisonous than source

define i1 @src(i32 %x) {
entry:
  %cmp1 = icmp ne i32 %x, 0
  %popcnt = call range(i32 1, 33) i32 @llvm.ctpop.i32(i32 %x)
  %cmp2 = icmp ult i32 %popcnt, 2
  %sel = select i1 %cmp1, i1 %cmp2, i1 false
  ret i1 %sel
}

define i1 @tgt(i32 %x) {
entry:
  %popcnt = call range(i32 1, 33) i32 @llvm.ctpop.i32(i32 %x)
  %sel = icmp eq i32 %popcnt, 1
  ret i1 %sel
}
