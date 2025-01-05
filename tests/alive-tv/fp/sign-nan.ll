define half @src1(half nofpclass(norm sub inf zero) noundef %x) {
  %fneg = fneg half %x
  ret half %fneg
}

define half @tgt1(half noundef %x) {
  ret half %x
}

define i1 @src2(half nofpclass(norm sub inf zero) noundef %x) {
  %cast = bitcast half %x to i16
  %cmp = icmp sgt i16 %cast, -1
  ret i1 %cmp
}

define i1 @tgt2(half noundef %x) {
  ret i1 true
}
