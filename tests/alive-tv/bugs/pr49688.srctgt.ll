define dso_local signext i32 @src(i32 signext %g, i32 zeroext %h) {
entry:
  %cmp = icmp slt i32 %g, 0
  %shr = ashr i32 7, %h
  %cmp1 = icmp sgt i32 %g, %shr
  %0 = select i1 %cmp, i1 true, i1 %cmp1
  %lor.ext = zext i1 %0 to i32
  ret i32 %lor.ext
}

define dso_local signext i32 @tgt(i32 signext %g, i32 zeroext %h) {
entry:
  %shr = lshr i32 7, %h
  %0 = icmp ult i32 %shr, %g
  %lor.ext = zext i1 %0 to i32
  ret i32 %lor.ext
}

; ERROR: Target is more poisonous than source
