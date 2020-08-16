define void @src(i8* %p) {
  %p2 = bitcast i8* %p to {}*
  store {} {}, {}* %p2
  ret void
}

define void @tgt(i8* %p) {
  ret void
}
