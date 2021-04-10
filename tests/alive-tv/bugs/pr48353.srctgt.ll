define i32 @src(i32 %aj) {
  %cmp.i = icmp sgt i32 %aj, 1
  %aj.op = lshr i32 3, %aj
  %.op2 = and i32 %aj.op, 1
  %cmp41 = icmp ne i32 %.op2, 0
  %cmp4 = select i1 %cmp.i, i1 1, i1 %cmp41
  %conv5 = zext i1 %cmp4 to i32
  ret i32 %conv5
}
define i32 @tgt(i32 %aj) {
  %cmp.i = icmp sgt i32 %aj, 1
  %aj.op = lshr i32 3, %aj
  %.op2 = and i32 %aj.op, 1
  %cmp41 = icmp ne i32 %.op2, 0
  %cmp4 = or i1 %cmp.i, %cmp41
  %conv5 = zext i1 %cmp4 to i32
  ret i32 %conv5
}

; ERROR: Target is more poisonous than source
