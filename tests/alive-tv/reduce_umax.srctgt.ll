declare i8 @llvm.experimental.vector.reduce.umax.v2i8(<2 x i8>)

define i8 @src(<2 x i8> %x) {
  %v0 = extractelement <2 x i8> %x, i8 0
  %v1 = extractelement <2 x i8> %x, i8 1
  %cmp = icmp ugt i8 %v0, %v1
  %r = select i1 %cmp, i8 %v0, i8 %v1
  ret i8 %r
}

define i8 @tgt(<2 x i8> %x) {
  %r = call i8 @llvm.experimental.vector.reduce.umax.v2i8(<2 x i8> %x)
  ret i8 %r
}
