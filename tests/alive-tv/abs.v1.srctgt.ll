declare i8 @llvm.abs.i8(i8, i1)

define i8 @src(i8 %v) {
  %neg_v = sub i8 0, %v
  %cmp = icmp slt i8 %v, 0
  %r = select i1 %cmp, i8 %neg_v, i8 %v
  ret i8 %r
}

define i8 @tgt(i8 %v0) {
  %r = call i8 @llvm.abs.i8(i8 %v0, i1 1)
  ret i8 %r
}

; ERROR: Target is more poisonous than source
